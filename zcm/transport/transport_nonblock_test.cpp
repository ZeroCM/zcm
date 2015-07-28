#include "zcm/transport.h"
#include "zcm/util/debug.h"

#include <cassert>
#include <cstring>

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportNonblockTest
#define MTU (1<<20)

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct ZCM_TRANS_CLASSNAME : public zcm_trans_nonblock_t
{
    char channel[33];
    char message[MTU];
    size_t messageSize;
    bool used = false;

    ZCM_TRANS_CLASSNAME(zcm_url_t *url)
    {
        vtbl = &methods;
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        // TODO shutdown
    }

    /********************** METHODS **********************/
    size_t getMtu()
    {
        return MTU;
    }

    int sendmsg(zcm_msg_t msg)
    {
        if (used)
            return ZCM_EAGAIN;

        strcpy(channel, msg.channel);
        memcpy(message, msg.buf, msg.len);
        messageSize = msg.len;
        used = true;

        return ZCM_EOK;
    }

    int recvmsgEnable(const char *channel, bool enable)
    {
        return ZCM_EOK;
    }

    int recvmsg(zcm_msg_t *msg)
    {
        if (!used)
            return ZCM_EAGAIN;

        msg->channel = channel;
        msg->len = messageSize;
        msg->buf = message;
        used = false;
        return ZCM_EOK;

    }

    int update()
    {
        return ZCM_EOK;
    }

    /********************** STATICS **********************/
    static zcm_trans_nonblock_methods_t methods;
    static ZCM_TRANS_CLASSNAME *cast(zcm_trans_nonblock_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _getMtu(zcm_trans_nonblock_t *zt)
    { return cast(zt)->getMtu(); }

    static int _sendmsg(zcm_trans_nonblock_t *zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static int _recvmsgEnable(zcm_trans_nonblock_t *zt, const char *channel, bool enable)
    { return cast(zt)->recvmsgEnable(channel, enable); }

    static int _recvmsg(zcm_trans_nonblock_t *zt, zcm_msg_t *msg)
    { return cast(zt)->recvmsg(msg); }

    static int _update(zcm_trans_nonblock_t *zt)
    { return cast(zt)->update(); }

    static void _destroy(zcm_trans_nonblock_t *zt)
    { delete cast(zt); }
};


zcm_trans_nonblock_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    &ZCM_TRANS_CLASSNAME::_update,
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static zcm_trans_nonblock_t *create(zcm_url_t *url)
{
    return new ZCM_TRANS_CLASSNAME(url);
}

// Register this transport with ZCM
static struct Register { Register() {
    zcm_transport_nonblock_register("nonblock-test", "Test impl for non-block transport", create);
}} reg;
