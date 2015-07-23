#include "transport.h"

#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
using namespace std;

// Define this the class name you want
#define ZCM_TRANS_NAME TransportZmqIpc
#define MTU (1<<20)

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
        return MTU;
    }

    int sendmsg(zcm_msg_t msg)
    {
        // XXX write me
        assert(0);
    }

    void recvmsg_enable(const char *channel, bool enable)
    {
        // XXX write me
        assert(0);
    }

    int recvmsg(zcm_msg_t *msg, int timeout)
    {
        // XXX write me
        assert(0);
    }

    void destory()
    {
        // XXX write me
        assert(0);
    }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME *cast(zcm_trans_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _get_mtu(zcm_trans_t *zt)
    { return cast(zt)->get_mtu(); }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static void _recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)
    { return cast(zt)->recvmsg_enable(channel, enable); }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
    { return cast(zt)->recvmsg(msg, timeout); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_get_mtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsg_enable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    &ZCM_TRANS_CLASSNAME::_destroy,
};

zcm_trans_t *zcm_trans_ipc_create()
{
    return new ZCM_TRANS_CLASSNAME();
}
