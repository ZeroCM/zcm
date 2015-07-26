#include "transport.h"
#include "debug.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

#include <cassert>
#include <cstring>

#include <string>
using namespace std;

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportSerial
#define MTU (1<<20)
#define ESCAPE_CHAR 0xcc

struct Serial
{
    Serial(){}
    ~Serial() { close(); }

    bool open(const std::string& port, int baud);
    bool isOpen() { return fd > 0; };
    void close();

    int write(const char *buf, size_t sz);
    int read(char *buf, size_t sz);
    static bool baudIsValid(int baud);

    Serial(const Serial&) = delete;
    Serial(Serial&&) = delete;
    Serial& operator=(const Serial&) = delete;
    Serial& operator=(Serial&&) = delete;

  private:
    int fd = -1;
};

bool Serial::open(const std::string& port, int baud)
{
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
    this->fd = -1;
    return false;
}

void Serial::close()
{
    if (isOpen())
        ::close(fd);
}

int Serial::write(const char *buf, size_t sz)
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

int Serial::read(char *buf, size_t sz)
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

    ZCM_TRANS_CLASSNAME()
    {
        vtbl = &methods;
        ser.open("/dev/pts/17", 115200);
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        // TODO shutdown serial
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

        char buffer[1024];
        size_t index = 0;
        uint8_t sum;  // TODO introduce better checksum

        auto writeBytes = [&](const char *data, size_t len) {
            for (size_t i = 0; i < len; i++) {
                // Less than 2 bytes of buffer left? Flush it.
                if (index >= sizeof(buffer)-1) {
                    ser.write(buffer, index);
                    index = 0;
                }
                char c = data[i];
                sum += (uint8_t)c;
                // Escape byte?
                if (c == ESCAPE_CHAR) {
                    buffer[index++] = c;
                    buffer[index++] = 0x00;
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
            ser.write((char*)&sum, 1);
        };

        // Sync bytes are two Escape Chars
        buffer[index++] = ESCAPE_CHAR;
        buffer[index++] = ESCAPE_CHAR;

        // Length of the channel (1 byte) due to ZCM_CHANNEL_MAXLEN
        // being less than 256
        static_assert(ZCM_CHANNEL_MAXLEN < (1<<8),
                      "Expected channel length to fit in one byte");
        buffer[index++] = (uint8_t)channel.size();

        // Length of the data (32-bits): Big Endian
        static_assert(MTU < (1ULL<<32),
                      "Expected data length to fit in 32-bits");
        uint32_t len = (uint32_t)msg.len;
        buffer[index++] = (len>>24)&0xff;
        buffer[index++] = (len>>16)&0xff;
        buffer[index++] = (len>>8)&0xff;
        buffer[index++] = (len>>0)&0xff;

        writeBytes(channel.c_str(), channel.size());
        writeBytes(msg.buf, msg.len);
        finish();

        return ZCM_EOK;
    }

    int recvmsgEnable(const char *channel, bool enable)
    {
        // WRITE ME
        assert(0);
    }

    int recvmsg(zcm_msg_t *msg, int timeout)
    {
        while(1)
            usleep(1000000);

        // WRITE ME
        assert(0);
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
    &ZCM_TRANS_CLASSNAME::_destroy,
};

extern "C" zcm_trans_t *zcm_trans_serial_create()
{
    return new ZCM_TRANS_CLASSNAME();
}
