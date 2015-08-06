#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/util/lockfile.h"
#include "zcm/util/debug.h"

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
#define MTU (1<<20)
#define ESCAPE_CHAR 0xcc

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct Serial
{
    Serial(){}
    ~Serial() { close(); }

    bool open(const string& port, int baud);
    bool isOpen() { return fd > 0; };
    void close();

    int write(const u8 *buf, size_t sz);
    int read(u8 *buf, size_t sz);
    static bool baudIsValid(int baud);

    Serial(const Serial&) = delete;
    Serial(Serial&&) = delete;
    Serial& operator=(const Serial&) = delete;
    Serial& operator=(Serial&&) = delete;

  private:
    string port;
    int fd = -1;
};

bool Serial::open(const string& port_, int baud)
{
    if (!baudIsValid(baud))
        return false;

    if (!lockfile_trylock(port_.c_str())) {
        ZCM_DEBUG("failed to create lock file, refusing to open serial device (%s)", port_.c_str());
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

    cfsetispeed(&opts, baud);
    cfsetospeed(&opts, baud);
    cfmakeraw(&opts);

    opts.c_cflag &= ~CSTOPB;
    opts.c_cflag |= CS8;
    opts.c_cflag &= ~PARENB;
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
    if(ret == -1) {
        ZCM_DEBUG("ERR: write failed: %s", strerror(errno));
        return -1;
    }
    fsync(fd);
    return ret;
}

int Serial::read(u8 *buf, size_t sz)
{
    assert(this->isOpen());
    int ret = ::read(fd, buf, sz);
    if(ret == -1) {
        ZCM_DEBUG("ERR: read failed: %s", strerror(errno));
    }
    return ret;
}

bool Serial::baudIsValid(int baud)
{
    switch (baud) {
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
            return true;
        default:
            return false;
    }
}

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    Serial ser;
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

        auto *baudStr = findOption("baud");
        if (!baudStr) {
            ZCM_DEBUG("requires a 'baud' argument");
            return;
        }

        int baud = atoi(baudStr->c_str());
        if (baud == 0) {
            ZCM_DEBUG("expected integer argument for 'baud'");
            return;
        }

        auto address = zcm_url_address(url);
        ser.open(address, baud);
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
                    ser.write(buffer, index);
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
                ser.write(buffer, index);
                index = 0;
            }
            ser.write(&sum, 1);
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
        u8 buffer[1024];
        size_t index = 0, size = 0;
        u8 sum = 0;  // TODO introduce better checksum

        auto refillBuffer = [&]() {
            while (index == size) {
                size = ser.read(buffer, sizeof(buffer));
                index = 0;
            }
        };
        auto readByte = [&]() {
            if (index == size)
                refillBuffer();
            return buffer[index++];
        };
        auto readU32 = [&]() {
            u32 a = readByte();
            u32 b = readByte();
            u32 c = readByte();
            u32 d = readByte();
            return a<<24 | b<<16 | c<<8 | d<<0;
        };
        auto syncStream = [&]() {
            while (1) {
                u8 c = readByte();
                if (c != ESCAPE_CHAR)
                    continue;
                // We got one escape char, see if it's
                // followed by a zero
                c = readByte();
                if (c == 0)
                    return;
            }
        };
        auto readByteUnescape = [&]() {
            u8 c = readByte();
            if (c != ESCAPE_CHAR)
                return c;
            // Byte was escaped, strip off the escape char
            return readByte();
        };
        auto readBytes = [&](u8 *buffer, size_t sz) {
            for (size_t i = 0; i < sz; i++) {
                u8 c = readByteUnescape();
                sum += c;
                buffer[i] = c;
            }
        };
        auto checkFinish = [&]() {
            u8 expect = readByte();
            return expect == sum;
        };

        syncStream();
        u8 channelLen = readByte();
        u32 dataLen = readU32();

        // Validate the lengths received
        if (channelLen > ZCM_CHANNEL_MAXLEN) {
            ZCM_DEBUG("serial recvmsg: channel is too long: %d", channelLen);
            // retry the recvmsg via tail-recursion
            return recvmsg(msg, timeout);
        }
        if (dataLen > MTU) {
            ZCM_DEBUG("serial recvmsg: data is too long: %d", dataLen);
            // retry the recvmsg via tail-recursion
            return recvmsg(msg, timeout);
        }

        // Lengths are good! Recv the data
        readBytes(recvChannelMem, (size_t)channelLen);
        readBytes(recvDataMem,    (size_t)dataLen);

        // Set the null-terminator
        recvChannelMem[channelLen] = '\0';

        // Check the checksum
        if (!checkFinish()) {
            ZCM_DEBUG("serial recvmsg: checksum failed!");
            // retry the recvmsg via tail-recursion
            return recvmsg(msg, timeout);
        }

        // Has this channel been enabled?
        if (!isChannelEnabled((char*)recvChannelMem)) {
            // retry the recvmsg via tail-recursion
            return recvmsg(msg, timeout);
        }

        // Good! Return it
        msg->channel = (char*)recvChannelMem;
        msg->len = dataLen;
        msg->buf = (char*)recvDataMem;

        return ZCM_EOK;
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

// Register this transport with ZCM
static struct Register { Register() {
    zcm_transport_register("serial", "Transfer data via a serial connection (e.g. 'serial:///dev/ttyUSB0?baud=115200')", create);
}} reg;
