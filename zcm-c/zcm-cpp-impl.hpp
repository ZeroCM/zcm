// DO NOT EVER INCLUDE THIS HEADER FILE YOURSELF

#ifndef __zcm_cpp_impl_ok__
#error "Don't include this file"
#endif

// =============== implementation ===============

inline ZCM::ZCM()
{
    zcm = zcm_create();
}

inline ZCM::~ZCM()
{
    zcm_destroy(zcm);
    zcm = nullptr;
}

inline bool ZCM::good() const
{
    return zcm != nullptr;
}

inline int ZCM::publish(const std::string& channel, const char *data,
                        uint len)
{
    return zcm_publish(zcm, channel.c_str(), (char*)data, len);
}

template <class Msg>
inline int ZCM::publish(const std::string& channel, const Msg *msg)
{
    uint datalen = msg->getEncodedSize();
    uint8_t *buf = new uint8_t[datalen];
    msg->encode(buf, 0, datalen);
    int status = this->publish(channel, (const char*)buf, datalen);
    delete[] buf;
    return status;
}

template <class Msg, class Handler>
class SubStub : public Subscription
{
    friend class ZCM;
  private:
    Handler* handler;
    void (Handler::*cb)(const ReceiveBuffer* rbuf, const std::string& channel, const Msg* msg);
    Msg msgMem; // Memory to decode this message into

    void dispatch(const zcm_recv_buf_t *rbuf, const char *channel)
    {
        int status = msgMem.decode(rbuf->data, 0, rbuf->len);
        if (status < 0) {
            fprintf (stderr, "error %d decoding %s!!!\n", status,
                     Msg::getTypeName());
            return;
        }
        const ReceiveBuffer rb = {
            rbuf->data,
            rbuf->len,
            rbuf->utime
        };
        std::string chan_str(channel);
        (handler->*cb)(&rb, chan_str, &msgMem);
    }

    // A standard vanilla function to bind the this-pointer
    static void dispatch_(const zcm_recv_buf_t *rbuf, const char *channel,
                          void *usr)
    {
        typedef SubStub<Msg, Handler> MyType;
        ((MyType*)usr)->dispatch(rbuf, channel);
    }
};

template <class Msg, class Handler>
Subscription *ZCM::subscribe(const std::string& channel,
                        void (Handler::*cb)(const ReceiveBuffer *rbuf, const std::string& channel, const Msg *msg),
                        Handler *handler)
{
    if (!zcm) {
        fprintf(stderr,
                "ZCM instance not initialized.  Ignoring call to subscribe()\n");
        return nullptr;
    }
    typedef SubStub<Msg, Handler> StubType;
    StubType *stub = new StubType();
    stub->handler = handler;
    stub->cb = cb;
    zcm_subscribe(zcm, channel.c_str(), StubType::dispatch_, stub);

    subscriptions.push_back(stub);
    return stub;
}

// template <class Handler>
// Subscription *ZCM::subscribe(const std::string& channel,
//                              void (Handler::*cb)(const ReceiveBuffer* rbuf, const std::string& channel),
//                              Handler* handler)
// {
// }

inline zcm_t *ZCM::getUnderlyingZCM()
{
    return zcm;
}
