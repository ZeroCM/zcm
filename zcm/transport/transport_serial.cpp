#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/transport_register.hpp"
#include "zcm/util/lockfile.h"
#include "zcm/util/debug.h"

#include "util/TimeUtil.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

#include <cassert>
#include <cstring>

#include <string>
#include <unordered_map>
#include <mutex>
using namespace std;

// TODO: This transport layer needs to be "hardened" to handle
// all of the possible errors and corner cases. Currently, it
// should work fine in most cases, but it might fail on some
// rare cases...

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportSerial
#define MTU (1<<14)
#define ESCAPE_CHAR 0xcc

#define SERIAL_TIMEOUT_US 1e5 // u-seconds

#define US_TO_MS(a) (a)/1e3

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct Serial
{
    Serial(){}
    ~Serial() { close(); }

    bool open(const string& port, int baud, bool hwFlowControl);
    bool isOpen() { return fd > 0; };
    void close();

    int write(const u8 *buf, size_t sz);
    int read(u8 *buf, size_t sz, u64 timeoutMs);
    // Returns 0 on invalid input baud otherwise returns termios constant baud value
    static int convertBaud(int baud);

    Serial(const Serial&) = delete;
    Serial(Serial&&) = delete;
    Serial& operator=(const Serial&) = delete;
    Serial& operator=(Serial&&) = delete;

  private:
    string port;
    int fd = -1;
};

bool Serial::open(const string& port_, int baud, bool hwFlowControl)
{
    if (baud == 0) {
        fprintf(stderr, "Serial baud rate not specified in url. "
                        "Proceeding without setting baud\n");
    } else if (!(baud = convertBaud(baud))) {
        return false;
    }

    if (!lockfile_trylock(port_.c_str())) {
        ZCM_DEBUG("failed to create lock file, refusing to open serial device (%s)",
                  port_.c_str());
        return false;
    }
    this->port = port_;

    int flags = O_RDWR | O_NOCTTY | O_SYNC;
    fd = ::open(port.c_str(), flags, 0);
    if(fd < 0) {
        ZCM_DEBUG("failed to open serial device (%s): %s", port.c_str(), strerror(errno));
        goto fail;
    }

    // attempt to reset the USB device
    // this call may fail: but we don't care
    // occasionally the USB bus can get in a weird state
    // this call will reset the USB in that situation
    ioctl(fd, USBDEVFS_RESET, 0);

    struct termios opts;

    // get the termios config
    if(tcgetattr(fd, &opts)) {
        ZCM_DEBUG("failed to get termios options on fd: %s", strerror(errno));
        goto fail;
    }

    if (baud != 0) {
        cfsetispeed(&opts, baud);
        cfsetospeed(&opts, baud);
    }
    cfmakeraw(&opts);

    opts.c_cflag &= ~CSTOPB;
    opts.c_cflag |= CS8;
    opts.c_cflag &= ~PARENB;
    if (hwFlowControl) opts.c_cflag |= CRTSCTS;
    opts.c_cc[VTIME]    = 1;
    opts.c_cc[VMIN]     = 30;

    // set the new termios config
    if(tcsetattr(fd, TCSANOW, &opts)) {
        ZCM_DEBUG("failed to set termios options on fd: %s", strerror(errno));
        goto fail;
    }

    tcflush(fd, TCIOFLUSH);

    return true;

 fail:
    // Close the port if it was opened
    if (fd > 0) {
        const int saved_errno = errno;
        int result;
        do {
            result = ::close(fd);
        } while (result == -1 && errno == EINTR);
        errno = saved_errno;
    }
    this->fd = -1;
    // Unlock the lock file
    if (port != "") {
        lockfile_unlock(port.c_str());
    }
    this->port = "";

    return false;
}

void Serial::close()
{
    if (isOpen()) {
        ZCM_DEBUG("Closing!\n");
        ::close(fd);
        fd = 0;
    }
    if (port != "") {
        lockfile_unlock(port.c_str());
        port = "";
    }
}

int Serial::write(const u8 *buf, size_t sz)
{
    assert(this->isOpen());
    int ret = ::write(fd, buf, sz);
    if (ret == -1) {
        ZCM_DEBUG("ERR: write failed: %s", strerror(errno));
        return -1;
    }
    fsync(fd);
    return ret;
}

int Serial::read(u8 *buf, size_t sz, u64 timeoutUs)
{
    assert(this->isOpen());
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    u64 tOut = max((u64)SERIAL_TIMEOUT_US, timeoutUs);

    struct timeval timeout;
    timeout.tv_sec = tOut / 1000000;
    timeout.tv_usec = tOut % 1000000;
    int status = ::select(fd + 1, &fds, NULL, NULL, &timeout);

    if (status > 0) {
        if (FD_ISSET(fd, &fds)) {
            int ret = ::read(fd, buf, sz);
            if (ret == -1) {
                ZCM_DEBUG("ERR: serial read failed: %s", strerror(errno));
            } else if (ret == 0) {
                ZCM_DEBUG("ERR: serial device unplugged");
                close();
                return -3;
            }
            return ret;
        } else {
            ZCM_DEBUG("ERR: serial bytes not ready");
            return -1;
        }
    } else {
        ZCM_DEBUG("ERR: serial read timed out");
        return -2;
    }
}

int Serial::convertBaud(int baud)
{
    switch (baud) {
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        default:
            return 0;
    }
}

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    Serial ser;
    int baud;
    bool hwFlowControl;
    string address;

    unordered_map<string, string> options;

    mutex mut; // protects the enabled channels
    unordered_map<string, int> recvChannels;
    bool recvAllChannels = false;

    // Preallocated memory for recv
    u8 recvChannelMem[33];
    u8 recvDataMem[MTU];

    string *findOption(const string& s)
    {
        auto it = options.find(s);
        if (it == options.end()) return nullptr;
        return &it->second;
    }

    bool isChannelEnabled(const char *channel)
    {
        unique_lock<mutex> lk(mut);
        if (recvAllChannels)
            return true;
        else
            return recvChannels[channel];
    }

    ZCM_TRANS_CLASSNAME(zcm_url_t *url)
    {
        trans_type = ZCM_BLOCKING;
        vtbl = &methods;

        // build 'options'
        auto *opts = zcm_url_opts(url);
        for (size_t i = 0; i < opts->numopts; i++)
            options[opts->name[i]] = opts->value[i];

        baud = 0;
        auto *baudStr = findOption("baud");
        if (!baudStr) {
            fprintf(stderr, "Baud unspecified. Bypassing serial baud setup.\n");
        } else {
            baud = atoi(baudStr->c_str());
            if (baud == 0) {
                ZCM_DEBUG("expected integer argument for 'baud'");
                return;
            }
        }

        hwFlowControl = false;
        auto *hwFlowControlStr = findOption("hw_flow_control");
        if (hwFlowControlStr) {
            if (*hwFlowControlStr == "true") {
                hwFlowControl = true;
            } else if (*hwFlowControlStr == "false") {
                hwFlowControl = false;
            } else {
                ZCM_DEBUG("expected boolean argument for 'hw_flow_control'");
                return;
            }
        }

        address = zcm_url_address(url);
        ser.open(address, baud, hwFlowControl);
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        // TODO shutdown serial
    }

    bool good()
    {
        return ser.isOpen();
    }

    /********************** METHODS **********************/
    size_t getMtu()
    {
        return MTU;
    }

    int sendmsg(zcm_msg_t msg)
    {
        string channel = msg.channel;
        if (channel.size() > ZCM_CHANNEL_MAXLEN)
            return ZCM_EINVALID;
        if (msg.len > MTU)
            return ZCM_EINVALID;

        u8 buffer[1024];
        size_t index = 0;
        u8 sum = 0;  // TODO introduce better checksum

        auto writeBytes = [&](const u8 *data, size_t len) {
            for (size_t i = 0; i < len; i++) {
                // Less than 2 bytes of buffer left? Flush it.
                if (index >= sizeof(buffer)-1) {
                    int ret = ser.write(buffer, index);
                    if (ret == -1) assert(false && "Serial port has been unplugged");
                    index = 0;
                }
                u8 c = data[i];
                sum += c;
                // Escape byte?
                if (c == ESCAPE_CHAR) {
                    buffer[index++] = ESCAPE_CHAR;
                    buffer[index++] = c;
                } else {
                    buffer[index++] = c;
                }
            }
        };

        auto finish = [&]() {
            if (index != 0) {
                int ret = ser.write(buffer, index);
                if (ret == -1) assert(false && "Serial port has been unplugged");
                index = 0;
            }
            int ret = ser.write(&sum, 1);
            if (ret == -1) assert(false && "Serial port has been unplugged");
        };

        // Sync bytes are Escape and 1 zero
        buffer[index++] = ESCAPE_CHAR;
        buffer[index++] = 0;

        // Length of the channel (1 byte) due to ZCM_CHANNEL_MAXLEN
        // being less than 256
        static_assert(ZCM_CHANNEL_MAXLEN < (1<<8),
                      "Expected channel length to fit in one byte");
        buffer[index++] = (uint8_t)channel.size();

        // Length of the data (32-bits): Big Endian
        static_assert(MTU < (1ULL<<32),
                      "Expected data length to fit in 32-bits");
        u32 len = (u32)msg.len;
        buffer[index++] = (len>>24)&0xff;
        buffer[index++] = (len>>16)&0xff;
        buffer[index++] = (len>>8)&0xff;
        buffer[index++] = (len>>0)&0xff;

        writeBytes((u8*)channel.c_str(), channel.size());
        writeBytes((u8*)msg.buf, msg.len);
        finish();

        return ZCM_EOK;
    }

    int recvmsgEnable(const char *channel, bool enable)
    {
        unique_lock<mutex> lk(mut);
        if (channel == NULL)
            recvAllChannels = enable;
        else
            recvChannels[channel] = (int)enable;
        return ZCM_EOK;
    }

    int recvmsg(zcm_msg_t *msg, int timeout)
    {
        // XXX DELETE THIS ONCE STATE IS MAINTAINED
        timeout = MTU * 2.2 * 8.0 / (baud == 0 ? 115200 : baud) * 1e3;
        // ---------------------------------------

        while (true) {
            if (timeout < 0) return ZCM_EAGAIN;

            u64 utimeRcvStart = TimeUtil::utime();

            u8 buffer[1024];
            size_t index = 0, size = 0;
            u8 sum = 0;  // TODO introduce better checksum
            bool timedOut = false;

            auto refillBuffer = [&]() {
                while (index == size) {
                    int n = 0;
                    if (!ser.isOpen()) {
                        ZCM_DEBUG("serial closed. Attempting reopen");
                        ser.open(address, baud, hwFlowControl);
                        usleep(timeout * 1e3);
                        assert(false && "Serial port has been unplugged");
                    } else {
                        n = ser.read(buffer, sizeof(buffer), timeout * 1e3);
                    }
                    if (n <= 0) {
                        ZCM_DEBUG("serial recvmsg: read timed out");
                        size = 0;
                        index = 0;
                        timedOut = true;
                        break;
                    } else {
                        index = 0;
                        size = n;
                    }
                }
            };
            auto readByte = [&](u8& b) {
                if (index == size) refillBuffer();
                if (size == 0) return false;
                b = buffer[index++];
                return true;
            };
            auto readU32 = [&](u32& x) {
                u8 a, b, c, d;
                if (readByte(a) && readByte(b) && readByte(c) && readByte(d)) {
                    x = (u32(a) << 24) | (u32(b) << 16) | (u32(c) << 8) | (u32(d) << 0);
                    return true;
                }
                return false;
            };
            auto syncStream = [&]() {
                while (!timedOut) {
                    u8 c;
                    if (!readByte(c)) return false;

                    if (c != ESCAPE_CHAR)
                        continue;

                    // We got one escape char, see if it's followed by a zero
                    if (!readByte(c)) return false;
                    if (c == 0) {
                        msg->utime = TimeUtil::utime();
                        return true;
                    }
                }
                return false;
            };
            auto readByteUnescape = [&](u8& b) {
                u8 c;
                if (!readByte(c)) return false;
                if (c != ESCAPE_CHAR) {
                    b = c;
                    return true;
                }
                // Byte was escaped, strip off the escape char
                return readByte(b);
            };
            auto readBytes = [&](u8 *buffer, size_t sz) {
                u8 c;
                for (size_t i = 0; i < sz; i++) {
                    if(!readByteUnescape(c)) return false;
                    sum += c;
                    buffer[i] = c;
                }
                return true;
            };
            auto checkFinish = [&]() {
                u8 expect;
                if (!readByte(expect)) return false;
                return expect == sum;
            };

            u8 channelLen;
            u32 dataLen;
            if (!syncStream() || !readByte(channelLen) || !readU32(dataLen))
                return ZCM_EAGAIN;

            int diff = US_TO_MS(TimeUtil::utime() - utimeRcvStart);

            // Validate the lengths received
            if (channelLen > ZCM_CHANNEL_MAXLEN) {
                ZCM_DEBUG("serial recvmsg: channel is too long: %d", channelLen);
                if (timeout > 0 && diff > timeout) return ZCM_EAGAIN;
                // try again
                timeout -= diff;
                continue;
            }
            if (dataLen > MTU) {
                ZCM_DEBUG("serial recvmsg: data is too long: %d", dataLen);
                if (timeout > 0 && diff > timeout) return ZCM_EAGAIN;
                // try again
                timeout -= diff;
                continue;
            }

            // Lengths are good! Recv the data
            if (!readBytes(recvChannelMem, (size_t)channelLen) ||
                !readBytes(recvDataMem,    (size_t)dataLen))
                return ZCM_EAGAIN;

            // Set the null-terminator
            recvChannelMem[channelLen] = '\0';

            diff = US_TO_MS(TimeUtil::utime() - utimeRcvStart);

            // Check the checksum
            if (!checkFinish()) {
                ZCM_DEBUG("serial recvmsg: checksum failed!");
                if (timeout > 0 && diff > timeout) return ZCM_EAGAIN;
                // try again
                timeout -= diff;
                continue;
            }

            // Has this channel been enabled?
            if (!isChannelEnabled((char*)recvChannelMem)) {
                //ZCM_DEBUG("serial recvmsg: %s is not enabled!", recvChannelMem);
                if (timeout > 0 && diff > timeout) return ZCM_EAGAIN;
                // try again
                timeout -= diff;
                continue;
            }

            // Good! Return it
            msg->channel = (char*)recvChannelMem;
            msg->len = dataLen;
            msg->buf = (char*)recvDataMem;

            return ZCM_EOK;
        }
    }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME *cast(zcm_trans_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _getMtu(zcm_trans_t *zt)
    { return cast(zt)->getMtu(); }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static int _recvmsgEnable(zcm_trans_t *zt, const char *channel, bool enable)
    { return cast(zt)->recvmsgEnable(channel, enable); }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
    { return cast(zt)->recvmsg(msg, timeout); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }

    static const TransportRegister reg;
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    NULL, // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static zcm_trans_t *create(zcm_url_t *url)
{
    auto *trans = new ZCM_TRANS_CLASSNAME(url);
    if (trans->good())
        return trans;

    delete trans;
    return nullptr;
}

#ifdef USING_TRANS_SERIAL
// Register this transport with ZCM
const TransportRegister ZCM_TRANS_CLASSNAME::reg(
    "serial", "Transfer data via a serial connection "
              "(e.g. 'serial:///dev/ttyUSB0?baud=115200&hw_flow_control=true')",
    create);
#endif
