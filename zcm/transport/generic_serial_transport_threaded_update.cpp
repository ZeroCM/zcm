#include "generic_serial_transport_threaded_update.h"

#include "zcm/transport.h"
#include "zcm/zcm.h"

#include "generic_serial_transport.h"

#include <atomic>
#include <cassert>
#include <thread>

#define ZCM_TRANS_CLASSNAME TransportGenericSerialThreadedUpdate

class ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
  public:
    ZCM_TRANS_CLASSNAME(size_t (*get)(uint8_t* data, size_t nData, uint32_t timeoutMs,
                                      void* usr),
                        size_t (*put)(const uint8_t* data, size_t nData,
                                      uint32_t timeoutMs, void* usr),
                        uint32_t timeoutMs, void* put_get_usr)
        : _get(get), _put(put), timeoutMs(timeoutMs), put_get_usr(put_get_usr)
    {
        trans_type = ZCM_BLOCKING;
        vtbl       = &methods;
    }
    ~ZCM_TRANS_CLASSNAME()
    {
        running = false;
        updateRxThr.join();
        updateTxThr.join();
        zcm_trans_destroy(gst);
    }

    void setGst(zcm_trans_t* zt) { gst = zt; }

    void updateRx()
    {
        while (running) { serial_update_rx(gst); }
    }

    void updateTx()
    {
        while (running) { serial_update_tx(gst); }
    }

    size_t getMtu() { return zcm_trans_get_mtu(gst); }
    int    sendmsg(zcm_msg_t msg) { return zcm_trans_sendmsg(gst, msg); }
    int    recvmsgEnable(const char* channel, bool enable)
    {
        return zcm_trans_recvmsg_enable(gst, channel, enable);
    }
    int recvmsg(zcm_msg_t* msg, unsigned timeout)
    {
        return zcm_trans_recvmsg(gst, msg, timeout);
    }

  private:
    zcm_trans_t* gst;

    size_t (*_get)(uint8_t* data, size_t nData, uint32_t timeoutMs, void* usr);
    size_t (*_put)(const uint8_t* data, size_t nData, uint32_t timeoutMs, void* usr);
    uint32_t timeoutMs;
    void*    put_get_usr;

    std::atomic<bool> running;
    std::thread       updateRxThr;
    std::thread       updateTxThr;

    /********************** STATICS **********************/
  public:
    static zcm_trans_methods_t  methods;
    static ZCM_TRANS_CLASSNAME* cast(zcm_trans_t* zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _getMtu(zcm_trans_t* zt) { return cast(zt)->getMtu(); }

    static int _sendmsg(zcm_trans_t* zt, zcm_msg_t msg) { return cast(zt)->sendmsg(msg); }

    static int _recvmsgEnable(zcm_trans_t* zt, const char* channel, bool enable)
    {
        return cast(zt)->recvmsgEnable(channel, enable);
    }

    static int _recvmsg(zcm_trans_t* zt, zcm_msg_t* msg, unsigned timeout)
    {
        return cast(zt)->recvmsg(msg, timeout);
    }

    static void _destroy(zcm_trans_t* zt) { delete cast(zt); }

    static size_t get(uint8_t* data, size_t nData, void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = (ZCM_TRANS_CLASSNAME*)usr;
        return me->_get(data, nData, me->timeoutMs, me->put_get_usr);
    }

    static size_t put(const uint8_t* data, size_t nData, void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = (ZCM_TRANS_CLASSNAME*)usr;
        return me->_put(data, nData, me->timeoutMs, me->put_get_usr);
    }
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    NULL, // drops
    NULL, // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

extern "C" {

zcm_trans_t* zcm_trans_generic_serial_threaded_update_create(
    size_t (*get)(uint8_t* data, size_t nData, uint32_t timeoutMs, void* usr),
    size_t (*put)(const uint8_t* data, size_t nData, uint32_t timeoutMs, void* usr),
    uint32_t timeoutMs, void* put_get_usr, uint64_t (*timestamp_now)(void* usr),
    void* time_usr, size_t MTU, size_t bufSize)
{
    ZCM_TRANS_CLASSNAME* zt = new ZCM_TRANS_CLASSNAME(get, put, timeoutMs, put_get_usr);

    zcm_trans_t* gst = zcm_trans_generic_serial_create(
        ZCM_TRANS_CLASSNAME::get, ZCM_TRANS_CLASSNAME::put, zt, timestamp_now, time_usr,
        MTU, bufSize);

    zt->setGst(gst);

    return zt;
}

void zcm_trans_generic_serial_threaded_update_destroy(zcm_trans_t* zt)
{
    ZCM_TRANS_CLASSNAME::_destroy(zt);
}
}
