#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/transport_register.hpp"

#include "zcm/zcm-cpp.hpp"

#include <cstdio>
#include <cassert>
#include <unordered_map>

#define ZCM_TRANS_NAME TransportFile
#define MTU (UNKNOWN RIGHT NOW)

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    LogFile *log = nullptr;
    unordered_map<string, string> options;

    string mode = "r";

    string *findOption(const string& s)
    {
        auto it = options.find(s);
        if (it == options.end()) return nullptr;
        return &it->second;
    }

    ZCM_TRANS_CLASSNAME(/* Add any methods here */)
    {
        trans_type = ZCM_BLOCKING;
        vtbl = &methods;

        // build 'options'
        auto *opts = zcm_url_opts(url);
        for (size_t i = 0; i < opts->numopts; i++)
            options[opts->name[i]] = opts->value[i];

        double speed = 1.0;
        string* speedStr = findOption("speed");
        if (speedStr) {
            speed = atod(baudStr->c_str());
            if (speed == 0) {
                ZCM_DEBUG("Expected double argument for 'speed'");
                return;
            }
        }

        string* modeStr = findOption("mode");
        if (modeStr) {
            mode = string(modeStr);
            if (!(mode == "r" || mode == "w" || mode == "a")) {
                ZCM_DEBUG("Expected r|w|a as mode argument for zcm file");
                return;
            }
        }

        auto filename = zcm_url_address(url);
        log = new LogFile(filename, string(mode));
        if (!log.good()) {
            cerr << "Unable to open logfile " << filename << endl;
            delete log;
            return;
        }
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        if (log) delete log;
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

    int recvmsg_enable(const char *channel, bool enable)
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

static zcm_trans_t *create(zcm_url_t *url)
{
    auto *trans = new ZCM_TRANS_CLASSNAME(url);
    if (trans->good())
        return trans;

    delete trans;
    return nullptr;
}

const TransportRegister ZCM_TRANS_CLASSNAME::regIpc(
    "file", "Interact with zcm log file (e.g. 'file://vehicle.log?speed=2.0)", create);
