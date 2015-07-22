#include "transport.h"
#include <cstdio>
#include <cassert>

// Define this the class name you want
#define ZCM_TRANS_NAME TransportZmqIpc

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    ZCM_TRANS_CLASSNAME(/* Add any methods here */)
    {
        vtbl = &methods;
    }

    ~ZCM_TRANS_CLASSNAME()
    {
    }

    /********************** METHODS **********************/
    size_t get_mtu()
    {
        // XXX write me
        assert(0);
    }

    zcm_sendmsg_t sendmsg_start(const char *channel, size_t sz, bool wait)
    {
        // XXX write me
        assert(0);
    }

    void sendmsg_finish()
    {
        // XXX write me
        assert(0);
    }

    int recvmsg_poll(int16_t timeout)
    {
        // XXX write me
        assert(0);
    }

    void recvmsg_enable(const char *channel, bool enable)
    {
        // XXX write me
        assert(0);
    }

    zcm_recvmsg_t recvmsg_start()
    {
        // XXX write me
        assert(0);
    }

    void recvmsg_finish()
    {
        // XXX write me
        assert(0);
    }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME *upcast(zcm_trans_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _get_mtu(zcm_trans_t *zt)
    { return upcast(zt)->get_mtu(); }

    static zcm_sendmsg_t _sendmsg_start(zcm_trans_t *zt, const char *channel, size_t sz, bool wait)
    { return upcast(zt)->sendmsg_start(channel, sz, wait); }

    static void _sendmsg_finish(zcm_trans_t *zt)
    { return upcast(zt)->sendmsg_finish(); }

    static int _recvmsg_poll(zcm_trans_t *zt, int16_t timeout)
    { return upcast(zt)->recvmsg_poll(timeout); }

    static void _recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)
    { return upcast(zt)->recvmsg_enable(channel, enable); }

    static zcm_recvmsg_t _recvmsg_start(zcm_trans_t *zt)
    { return upcast(zt)->recvmsg_start(); }

    static void _recvmsg_finish(zcm_trans_t *zt)
    { return upcast(zt)->recvmsg_finish(); }

    static void _destroy(zcm_trans_t *zt)
    { delete upcast(zt); }
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_get_mtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg_start,
    &ZCM_TRANS_CLASSNAME::_sendmsg_finish,
    &ZCM_TRANS_CLASSNAME::_recvmsg_poll,
    &ZCM_TRANS_CLASSNAME::_recvmsg_enable,
    &ZCM_TRANS_CLASSNAME::_recvmsg_start,
    &ZCM_TRANS_CLASSNAME::_recvmsg_finish,
    &ZCM_TRANS_CLASSNAME::_destroy,
};

zcm_trans_t *zcm_trans_ipc_create()
{
    return new ZCM_TRANS_CLASSNAME();
}
