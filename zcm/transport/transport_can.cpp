#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/transport_register.hpp"

#include <iostream>
#include <cstdio>
#include <cassert>

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportCan
#define MTU (1<<14)

using namespace std;

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    ZCM_TRANS_CLASSNAME(/* Add any args here (e.g. 'const char* url' */)
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
        // WRITE ME
        assert(0);
    }

    int recvmsg_enable(const char *channel, bool enable)
    {
        // WRITE ME
        assert(0);
    }

    int recvmsg(zcm_msg_t *msg, int timeout)
    {
        // WRITE ME
        assert(0);
    }

    void destory()
    {
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

    static size_t _get_mtu(zcm_trans_t *zt)
    { return cast(zt)->get_mtu(); }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static int _recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)
    { return cast(zt)->recvmsg_enable(channel, enable); }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
    { return cast(zt)->recvmsg(msg, timeout); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }

    /** If you choose to use the registrar, use a static registration member **/
    static const TransportRegister reg;
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_get_mtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsg_enable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    NULL,
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static zcm_trans_t *create(zcm_url_t* url)
{
    cout << "af2asdfasd" << endl;
    return new ZCM_TRANS_CLASSNAME();
}

const TransportRegister ZCM_TRANS_CLASSNAME::reg(
    "can", "Transdfs description (especially url components)", create);
