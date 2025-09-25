#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/transport_register.hpp"
#include "zcm/util/lockfile.h"
#include "zcm/util/debug.h"

#include "generic_serial_transport.h"

#include "util/TimeUtil.hpp"

#include <cassert>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <mutex>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace std;

// TODO: This transport layer needs to be "hardened" to handle
// all of the possible errors and corner cases. Currently, it
// should work fine in most cases, but it might fail on some
// rare cases...

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportSerial
#define MTU (1<<14)
#define ESCAPE_CHAR (0xcc)

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

    int write(const u8* buf, size_t sz);
    int read(u8* buf, size_t sz, u64 timeoutUs);
    // Returns 0 on invalid input baud otherwise returns termios constant baud value
    static int convertBaud(int baud);

    Serial(const Serial&) = delete;
    Serial(Serial&&) = delete;
    Serial& operator=(const Serial&) = delete;
    Serial& operator=(Serial&&) = delete;

  private:
    string port;
    int fd = -1;
    lockfile_t* lf;
};

bool Serial::open(const string& port_, int baud, bool hwFlowControl)
{
    if (baud == 0) {
        fprintf(stderr, "Serial baud rate not specified in url. "
                        "Proceeding without setting baud\n");
    } else if (!(baud = convertBaud(baud))) {
        fprintf(stderr, "Unrecognized baudrate. Failed to open serial device.\n ");
        return false;
    }

    lf = lockfile_trylock(port_.c_str());
    if (!lf) {
        ZCM_DEBUG("failed to create lock file, refusing to open serial device (%s)",
                  port_.c_str());
        return false;
    }
    this->port = port_;

    int flags = O_RDWR | O_NOCTTY | O_SYNC;
    fd = ::open(port.c_str(), flags, 0);
    if (fd < 0) {
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
    if (tcgetattr(fd, &opts)) {
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
    if (tcsetattr(fd, TCSANOW, &opts)) {
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
    if (lf) {
        lockfile_unlock(lf);
        lf = nullptr;
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
    if (port != "") port = "";
    if (lf) {
        lockfile_unlock(lf);
        lf = nullptr;
    }
}

int Serial::write(const u8* buf, size_t sz)
{
    assert(this->isOpen());
    int ret = ::write(fd, buf, sz);
    if (ret == -1) {
        ZCM_DEBUG("ERR: write failed: %s", strerror(errno));
        return -1;
    }
    return ret;
}

int Serial::read(u8* buf, size_t sz, u64 timeoutUs)
{
    assert(this->isOpen());
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    struct timeval timeout;
    timeout.tv_sec = timeoutUs / 1000000;
    timeout.tv_usec = timeoutUs - timeout.tv_sec * 1000000;
    int status = ::select(fd + 1, &fds, NULL, NULL, &timeout);

    if (status > 0) {
        if (FD_ISSET(fd, &fds)) {
            int ret = ::read(fd, buf, sz);
            if (ret == -1) {
                ZCM_DEBUG("ERR: serial read failed: %s", strerror(errno));
            } else if (ret == 0) {
                ZCM_DEBUG("ERR: serial device unplugged");
                close();
                assert(false && "ERR: serial device unplugged\n" &&
                       "ZCM does not support reconnecting to serial devices");
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
        case 460800:
            return B460800;
        default:
            return 0;
    }
}

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    Serial ser;

    int baud;
    bool hwFlowControl;

    string rawChan;
    std::vector<uint8_t> rawBuf;

    string address;

    unordered_map<string, string> options;

    zcm_trans_t* gst;

    uint64_t timeoutLeftUs;

    string* findOption(const string& s)
    {
        auto it = options.find(s);
        if (it == options.end()) return nullptr;
        return &it->second;
    }

    ZCM_TRANS_CLASSNAME(zcm_url_t* url)
    {
        trans_type = ZCM_BLOCKING;

        // build 'options'
        auto* opts = zcm_url_opts(url);
        for (size_t i = 0; i < opts->numopts; ++i)
            options[opts->name[i]] = opts->value[i];

        baud = 0;
        auto* baudStr = findOption("baud");
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
        auto* hwFlowControlStr = findOption("hw_flow_control");
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

        bool raw = false;
        auto* rawStr = findOption("raw");
        if (rawStr) {
            if (*rawStr == "true") {
                raw = true;
            } else if (*rawStr == "false") {
                raw = false;
            } else {
                ZCM_DEBUG("expected boolean argument for 'raw'");
                return;
            }
        }

        rawChan = "";
        auto* rawChanStr = findOption("raw_channel");
        if (rawChanStr) {
            rawChan = *rawChanStr;
        }

        size_t rawSize = 1024;
        auto* rawSizeStr = findOption("raw_size");
        if (rawSizeStr) {
            rawSize = atoi(rawSizeStr->c_str());
            if (rawSize <= 0) {
                ZCM_DEBUG("expected positive integer argument for 'raw_size'");
                return;
            }
        }

        address = zcm_url_address(url);
        ser.open(address, baud, hwFlowControl);

        if (raw) {
            vtbl = &rawMethods;
            rawBuf.resize(rawSize);
            gst = nullptr;
        } else {
            vtbl = &methods;
            gst = zcm_trans_generic_serial_create(&ZCM_TRANS_CLASSNAME::get,
                                                  &ZCM_TRANS_CLASSNAME::put,
                                                  this,
                                                  &ZCM_TRANS_CLASSNAME::timestamp_now,
                                                  nullptr,
                                                  MTU, MTU * 10);
        }
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        ser.close();
        if (gst) zcm_trans_generic_serial_destroy(gst);
    }

    bool good()
    {
        return ser.isOpen();
    }

    static size_t get(uint8_t* data, size_t nData, void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = cast((zcm_trans_t*) usr);
        uint64_t startUtime = TimeUtil::utime();
        int ret = me->ser.read(data, nData, me->timeoutLeftUs);
        uint64_t diff = TimeUtil::utime() - startUtime;
        me->timeoutLeftUs = me->timeoutLeftUs > diff ? me->timeoutLeftUs - diff : 0;
        return ret < 0 ? 0 : ret;
    }

    static size_t put(const uint8_t* data, size_t nData, void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = cast((zcm_trans_t*) usr);
        int ret = me->ser.write(data, nData);
        return ret < 0 ? 0 : ret;
    }

    static uint64_t timestamp_now(void* usr)
    { return TimeUtil::utime(); }

    /********************** METHODS **********************/
    size_t getMtuRaw()
    { return MTU; }

    size_t getMtu()
    { return zcm_trans_get_mtu(this->gst); }

    int sendmsgRaw(zcm_msg_t msg)
    {
        if (!msg.len) return ZCM_EOK;
        do {
            int ret = ser.write(msg.buf, msg.len);

            if (ret < 0) break;
            if ((size_t)ret == msg.len) return ZCM_EOK;

            msg.buf += ret;
            msg.len -= ret;
        } while (true);

        return ZCM_EUNKNOWN;
    }

    int sendmsg(zcm_msg_t msg)
    {
        // Note: No need to lock here ONLY because the internals of
        //       generic serial transport sendmsg only use the sendBuffer
        //       and touch no variables related to receiving
        int ret = zcm_trans_sendmsg(this->gst, msg);
        if (ret != ZCM_EOK) return ret;
        while (true) {
            ret = serial_update_tx(this->gst);
            if (ret != ZCM_EAGAIN) break;
        }
        return ret;
    }

    int recvmsgEnableRaw(const char* channel, bool enable)
    { return ZCM_EOK; }

    int recvmsgEnable(const char* channel, bool enable)
    { return zcm_trans_recvmsg_enable(this->gst, channel, enable); }

    int recvmsgRaw(zcm_msg_t* msg, unsigned timeoutMs)
    {
        // used by get()
        timeoutLeftUs = timeoutMs * 1e3;

        size_t sz = get(rawBuf.data(), rawBuf.size(), this);
        if (sz == 0 || rawChan.empty()) return ZCM_EAGAIN;

        msg->utime   = timestamp_now(this);
        msg->channel = rawChan.c_str();
        msg->len     = sz;
        msg->buf     = rawBuf.data();

        return ZCM_EOK;
    }

    int recvmsg(zcm_msg_t* msg, unsigned timeoutMs)
    {
        uint64_t endUtime = TimeUtil::utime() + timeoutMs * 1e3;

        do {

            // Note: No need to lock here ONLY because the internals of
            //       generic serial transport recvmsg only use the recv related
            //       data members and touch no variables related to sending
            int ret = zcm_trans_recvmsg(this->gst, msg, 0);
            if (ret == ZCM_EOK) return ret;

            // Note: timeoutLeftUs is calculated here because serial_update_rx
            //       needs it to be set properly so that the blocking read in
            //       `get` knows how long it has to exit
            uint64_t now = TimeUtil::utime();
            timeoutLeftUs = endUtime < now ? 0 : endUtime - now;
            serial_update_rx(this->gst);

        } while (TimeUtil::utime() < endUtime);

        return ZCM_EAGAIN;
    }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods, rawMethods;
    static ZCM_TRANS_CLASSNAME* cast(zcm_trans_t* zt)
    {
        assert(zt->vtbl == &rawMethods || zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _getMtuRaw(zcm_trans_t* zt)
    { return cast(zt)->getMtuRaw(); }

    static size_t _getMtu(zcm_trans_t* zt)
    { return cast(zt)->getMtu(); }

    static int _sendmsgRaw(zcm_trans_t* zt, zcm_msg_t msg)
    { return cast(zt)->sendmsgRaw(msg); }

    static int _sendmsg(zcm_trans_t* zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static int _recvmsgEnableRaw(zcm_trans_t* zt, const char* channel, bool enable)
    { return cast(zt)->recvmsgEnableRaw(channel, enable); }

    static int _recvmsgEnable(zcm_trans_t* zt, const char* channel, bool enable)
    { return cast(zt)->recvmsgEnable(channel, enable); }

    static int _recvmsgRaw(zcm_trans_t* zt, zcm_msg_t* msg, unsigned timeout)
    { return cast(zt)->recvmsgRaw(msg, timeout); }

    static int _recvmsg(zcm_trans_t* zt, zcm_msg_t* msg, unsigned timeout)
    { return cast(zt)->recvmsg(msg, timeout); }

    static void _destroy(zcm_trans_t* zt)
    { delete cast(zt); }

    static const TransportRegister reg;
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    NULL, // drops
    NULL, // set_queue_size
    NULL, // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::rawMethods = {
    &ZCM_TRANS_CLASSNAME::_getMtuRaw,
    &ZCM_TRANS_CLASSNAME::_sendmsgRaw,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnableRaw,
    &ZCM_TRANS_CLASSNAME::_recvmsgRaw,
    NULL, // drops
    NULL, // set_queue_size
    NULL, // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static zcm_trans_t* create(zcm_url_t* url, char **opt_errmsg)
{
    if (opt_errmsg) *opt_errmsg = NULL; // Feature unused in this transport
    auto* trans = new ZCM_TRANS_CLASSNAME(url);
    if (!trans->good()) {
        delete trans;
        return nullptr;
    }
    return trans;
}

#ifdef USING_TRANS_SERIAL
// Register this transport with ZCM
const TransportRegister ZCM_TRANS_CLASSNAME::reg(
    "serial", "Transfer data via a serial connection "
              "(e.g. 'serial:///dev/ttyUSB0?baud=115200&hw_flow_control=true' or "
              "'serial:///dev/pts/10?raw=true&raw_channel=RAW_SERIAL')",
    create);
#endif
