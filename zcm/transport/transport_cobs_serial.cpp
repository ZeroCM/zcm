#include "generic_serial_transport.h"
#include "util/TimeUtil.hpp"
#include "zcm/transport.h"
#include "zcm/transport/cobs_serial/generic_serial_cobs_transport.h"
#include "zcm/transport/transport_serial.hpp"
#include "zcm/transport_register.hpp"
#include "zcm/transport_registrar.h"
#include "zcm/util/debug.h"
#include "zcm/util/lockfile.h"
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#include <unordered_map>

#include <cassert>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <termios.h>
#include <unistd.h>
using namespace std;

// TODO: This transport layer needs to be "hardened" to handle
// all of the possible errors and corner cases. Currently, it
// should work fine in most cases, but it might fail on some
// rare cases...

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportCobsSerial
#define MTU (1 << 14)
#define ESCAPE_CHAR (0xcc)

#define SERIAL_TIMEOUT_US 1e5  // u-seconds

#define US_TO_MS(a) (a) / 1e3

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t {
    Serial ser;

    int baud;
    bool hwFlowControl;

    bool raw;
    string rawChan;
    int rawSize;
    std::unique_ptr<uint8_t[]> rawBuf;

    string address;

    unordered_map<string, string> options;

    zcm_trans_t* gst;

    uint64_t timeoutLeft;

    string* findOption(const string& s)
    {
        auto it = options.find(s);
        if (it == options.end()) return nullptr;
        return &it->second;
    }

    ZCM_TRANS_CLASSNAME(zcm_url_t* url)
    {
        trans_type = ZCM_BLOCKING;
        vtbl = &methods;

        // build 'options'
        auto* opts = zcm_url_opts(url);
        for (size_t i = 0; i < opts->numopts; ++i)
            options[opts->name[i]] = opts->value[i];

        baud = 0;
        auto* baudStr = findOption("baud");
        if (!baudStr) {
            fprintf(stderr, "Baud unspecified. Bypassing serial baud setup.\n");
        }
        else {
            baud = atoi(baudStr->c_str());
            if (baud == 0) {
                ZCM_DEBUG("expected integer argument for 'baud'");
                return;
            }
        }

        hwFlowControl = false;
        auto* hwFlowControlStr = findOption("hw_flow_control");
        if (hwFlowControlStr) {
            if (*hwFlowControlStr == "true") { hwFlowControl = true; }
            else if (*hwFlowControlStr == "false") {
                hwFlowControl = false;
            }
            else {
                ZCM_DEBUG("expected boolean argument for 'hw_flow_control'");
                return;
            }
        }

        raw = false;
        auto* rawStr = findOption("raw");
        if (rawStr) {
            if (*rawStr == "true") { raw = true; }
            else if (*rawStr == "false") {
                raw = false;
            }
            else {
                ZCM_DEBUG("expected boolean argument for 'raw'");
                return;
            }
        }

        rawChan = "";
        auto* rawChanStr = findOption("raw_channel");
        if (rawChanStr) { rawChan = *rawChanStr; }

        rawSize = 1024;
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
            rawBuf.reset(new uint8_t[rawSize]);
            gst = nullptr;
        }
        else {
            gst = zcm_trans_generic_serial_cobs_create(
                &ZCM_TRANS_CLASSNAME::get, &ZCM_TRANS_CLASSNAME::put, this,
                &ZCM_TRANS_CLASSNAME::timestamp_now, nullptr, MTU, MTU * 10);
        }
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        ser.close();
        if (gst) zcm_trans_generic_serial_cobs_destroy(gst);
    }

    bool good() { return ser.isOpen(); }

    static size_t get(uint8_t* data, size_t nData, void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = cast((zcm_trans_t*)usr);
        uint64_t startUtime = TimeUtil::utime();
        int ret = me->ser.read(data, nData, me->timeoutLeft);
        uint64_t diff = TimeUtil::utime() - startUtime;
        me->timeoutLeft = me->timeoutLeft > diff ? me->timeoutLeft - diff : 0;
        return ret < 0 ? 0 : ret;
    }

    static size_t put(const uint8_t* data, size_t nData, void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = cast((zcm_trans_t*)usr);
        int ret = me->ser.write(data, nData);
        return ret < 0 ? 0 : ret;
    }

    static uint64_t timestamp_now(void* usr) { return TimeUtil::utime(); }

    /********************** METHODS **********************/
    size_t getMtu() { return raw ? MTU : zcm_trans_get_mtu(this->gst); }

    int sendmsg(zcm_msg_t msg)
    {
        if (raw) {
            if (put(msg.buf, msg.len, this) != 0) return ZCM_EOK;
            return ZCM_EAGAIN;
        }
        else {
            // Note: No need to lock here ONLY because the internals of
            //       generic serial transport sendmsg only use the sendBuffer
            //       and touch no variables related to receiving
            int ret = zcm_trans_sendmsg(this->gst, msg);
            if (ret != ZCM_EOK) return ret;
            return serial_cobs_update_tx(this->gst);
        }
    }

    int recvmsgEnable(const char* channel, bool enable)
    {
        return raw ? ZCM_EOK
                   : zcm_trans_recvmsg_enable(this->gst, channel, enable);
    }

    int recvmsg(zcm_msg_t* msg, int timeoutMs)
    {
        timeoutLeft =
            timeoutMs > 0 ? timeoutMs * 1e3 : numeric_limits<uint64_t>::max();

        if (raw) {
            size_t sz = get(rawBuf.get(), rawSize, this);
            if (sz == 0 || rawChan.empty()) return ZCM_EAGAIN;

            msg->utime = timestamp_now(this);
            msg->channel = rawChan.c_str();
            msg->len = sz;
            msg->buf = rawBuf.get();

            return ZCM_EOK;
        }
        else {
            do {
                uint64_t startUtime = TimeUtil::utime();

                // Note: No need to lock here ONLY because the internals of
                //       generic serial transport recvmsg only use the recv
                //       related data members and touch no variables related to
                //       sending
                int ret = zcm_trans_recvmsg(this->gst, msg, timeoutLeft);
                if (ret == ZCM_EOK) return ret;

                uint64_t diff = TimeUtil::utime() - startUtime;
                startUtime = TimeUtil::utime();
                // Note: timeoutLeft is calculated here because serial_update_rx
                //       needs it to be set properly so that the blocking read
                //       in `get` knows how long it has to exit
                timeoutLeft = timeoutLeft > diff ? timeoutLeft - diff : 0;

                serial_cobs_update_rx(this->gst);

                diff = TimeUtil::utime() - startUtime;
                timeoutLeft = timeoutLeft > diff ? timeoutLeft - diff : 0;

            } while (timeoutLeft > 0);

            return ZCM_EAGAIN;
        }
    }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME* cast(zcm_trans_t* zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _getMtu(zcm_trans_t* zt) { return cast(zt)->getMtu(); }

    static int _sendmsg(zcm_trans_t* zt, zcm_msg_t msg)
    {
        return cast(zt)->sendmsg(msg);
    }

    static int _recvmsgEnable(zcm_trans_t* zt, const char* channel, bool enable)
    {
        return cast(zt)->recvmsgEnable(channel, enable);
    }

    static int _recvmsg(zcm_trans_t* zt, zcm_msg_t* msg, unsigned timeout)
    {
        return cast(zt)->recvmsg(msg, timeout);
    }

    static void _destroy(zcm_trans_t* zt) { delete cast(zt); }

    static const TransportRegister reg;
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    NULL,  // drops
    NULL,  // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static zcm_trans_t* create(zcm_url_t* url, char** opt_errmsg)
{
    if (opt_errmsg) *opt_errmsg = NULL;  // Feature unused in this transport
    auto* trans = new ZCM_TRANS_CLASSNAME(url);
    if (trans->good()) return trans;

    delete trans;
    return nullptr;
}

#ifdef USING_TRANS_SERIAL
// Register this transport with ZCM
const TransportRegister ZCM_TRANS_CLASSNAME::reg(
    "serial-cobs",
    "Transfer data via a serial connection using COBS encoding "
    "(e.g. 'serial-cobs:///dev/ttyUSB0?baud=115200&hw_flow_control=true' or "
    "'serial-cobs:///dev/pts/10?raw=true&raw_channel=RAW_SERIAL')",
    create);
#endif
