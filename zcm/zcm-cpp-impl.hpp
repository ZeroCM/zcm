// DO NOT EVER INCLUDE THIS HEADER FILE YOURSELF

#ifndef __zcm_cpp_impl_ok__
#error "Don't include this file"
#endif

#ifdef ZCM_EMBEDDED
#define nullptr NULL
#endif

// =============== implementation ===============

// Note: To prevent compiler "redefinition" issues, all functions in this file must be declared
//       as `inline`

inline ZCM::ZCM()
{
    zcm = zcm_create(nullptr);
}

inline ZCM::ZCM(const std::string& transport)
{
    zcm = zcm_create(transport.c_str());
}

inline ZCM::ZCM(zcm_trans_t *zt)
{
    zcm = (zcm_t*) malloc(sizeof(zcm_t));
    zcm_init_trans(zcm, zt);
}

inline ZCM::~ZCM()
{
    if (zcm != nullptr)
        zcm_destroy(zcm);

    std::vector<Subscription*>::iterator end = subscriptions.end(),
                                          it = subscriptions.begin();
    for (;it != end; ++it) delete *it;

    zcm = nullptr;
}

inline bool ZCM::good() const
{
    return zcm != nullptr;
}

inline int ZCM::err()
{
    return zcm_errno(zcm);
}

inline const char *ZCM::strerror()
{
    return zcm_strerror(zcm);
}

inline void ZCM::run()
{
    zcm_run(zcm);
}

inline void ZCM::start()
{
    zcm_start(zcm);
}

inline void ZCM::stop()
{
    zcm_stop(zcm);
}

inline int ZCM::handle()
{
    return zcm_handle(zcm);
}

inline int ZCM::handleNonblock()
{
    return zcm_handle_nonblock(zcm);
}

inline void ZCM::flush()
{
    zcm_flush(zcm);
}

inline int ZCM::publish(const std::string& channel, const char *data, uint32_t len)
{
    return zcm_publish(zcm, channel.c_str(), (char*)data, len);
}

template <class Msg>
inline int ZCM::publish(const std::string& channel, const Msg *msg)
{
    uint32_t len = msg->getEncodedSize();
    uint8_t *buf = new uint8_t[len];
    msg->encode(buf, 0, len);
    int status = this->publish(channel, (const char*)buf, len);
    delete[] buf;
    return status;
}

// Virtual inheritance to avoid ambiguous base class problem http://stackoverflow.com/a/139329
template<class Msg>
class TypedSubscription : public virtual Subscription
{
    friend class ZCM;

  protected:
    void (*typedCallback)(const ReceiveBuffer* rbuf, const std::string& channel, const Msg* msg,
                          void *usr);
    Msg msgMem; // Memory to decode this message into

  public:
    virtual ~TypedSubscription() {}

    inline int readMsg(const ReceiveBuffer *rbuf)
    {
        int status = msgMem.decode(rbuf->data, 0, rbuf->data_size);
        if (status < 0) {
            fprintf (stderr, "error %d decoding %s!!!\n", status, Msg::getTypeName());
            return -1;
        }
        return 0;
    }

    inline void typedDispatch(const ReceiveBuffer *rbuf, const char *channel)
    {
        if (readMsg(rbuf) != 0) return;
        (*typedCallback)(rbuf, channel, &msgMem, usr);
    }

    static inline void dispatch(const ReceiveBuffer *rbuf, const char *channel, void *usr)
    {
        ((TypedSubscription<Msg>*)usr)->typedDispatch(rbuf, channel);
    }
};

// Virtual inheritance to avoid ambiguous base class problem http://stackoverflow.com/a/139329
template <class Handler>
class HandlerSubscription : public virtual Subscription
{
    friend class ZCM;

  protected:
    Handler* handler;
    void (Handler::*handlerCallback)(const ReceiveBuffer* rbuf, const std::string& channel);

  public:
    virtual ~HandlerSubscription() {}

    inline void handlerDispatch(const ReceiveBuffer *rbuf, const char *channel)
    {
        (handler->*handlerCallback)(rbuf, channel);
    }

    static inline void dispatch(const ReceiveBuffer *rbuf, const char *channel, void *usr)
    {
        ((HandlerSubscription<Handler>*)usr)->handlerDispatch(rbuf, channel);
    }
};

template <class Msg, class Handler>
class TypedHandlerSubscription : public TypedSubscription<Msg>, HandlerSubscription<Handler>
{
    friend class ZCM;

  protected:
    void (Handler::*typedHandlerCallback)(const ReceiveBuffer* rbuf, const std::string& channel,
                                          const Msg* msg);

  public:
    virtual ~TypedHandlerSubscription() {}

    inline void typedHandlerDispatch(const ReceiveBuffer *rbuf, const char *channel)
    {
        // Unfortunately, we need to add "this" here to handle template inheritance:
        // https://isocpp.org/wiki/faq/templates#nondependent-name-lookup-members
        if (this->readMsg(rbuf) != 0) return;
        (this->handler->*typedHandlerCallback)(rbuf, channel, &this->msgMem);
    }

    static inline void dispatch(const ReceiveBuffer *rbuf, const char *channel, void *usr)
    {
        ((TypedHandlerSubscription<Msg, Handler>*)usr)->typedHandlerDispatch(rbuf, channel);
    }

};

// TODO: lots of room to condense the implementations of the various subscribe functions
template <class Msg, class Handler>
inline Subscription *ZCM::subscribe(const std::string& channel,
                                    void (Handler::*cb)(const ReceiveBuffer *rbuf,
                                                        const std::string& channel,
                                                        const Msg *msg),
                                    Handler *handler)
{
    if (!zcm) {
        fprintf(stderr, "ZCM instance not initialized. Ignoring call to subscribe()\n");
        return nullptr;
    }

    typedef TypedHandlerSubscription<Msg, Handler> SubType;
    SubType *sub = new SubType();
    sub->handler = handler;
    sub->typedHandlerCallback = cb;
    sub->c_sub = zcm_subscribe(zcm, channel.c_str(), SubType::dispatch, sub);

    subscriptions.push_back(sub);
    return sub;
}

template <class Handler>
inline Subscription *ZCM::subscribe(const std::string& channel,
                                    void (Handler::*cb)(const ReceiveBuffer* rbuf,
                                                        const std::string& channel),
                                    Handler* handler)
{
    if (!zcm) {
        fprintf(stderr, "ZCM instance not initialized.  Ignoring call to subscribe()\n");
        return nullptr;
    }

    typedef HandlerSubscription<Handler> SubType;
    SubType *sub = new SubType();
    sub->handler = handler;
    sub->handlerCallback = cb;
    sub->c_sub = zcm_subscribe(zcm, channel.c_str(), SubType::dispatch, sub);

    subscriptions.push_back(sub);
    return sub;
}

template <class Msg>
inline Subscription *ZCM::subscribe(const std::string& channel,
                                    void (*cb)(const ReceiveBuffer *rbuf,
                                               const std::string& channel,
                                               const Msg *msg, void *usr),
                                    void *usr)
{
    if (!zcm) {
        fprintf(stderr, "ZCM instance not initialized.  Ignoring call to subscribe()\n");
        return nullptr;
    }

    typedef TypedSubscription<Msg> SubType;
    SubType *sub = new SubType();
    sub->usr = usr;
    sub->typedCallback = cb;
    sub->c_sub = zcm_subscribe(zcm, channel.c_str(), SubType::dispatch, sub);

    subscriptions.push_back(sub);
    return sub;
}

#if __cplusplus > 199711L
template <class Msg>
inline void __zcm__cbHelper(const ReceiveBuffer *rbuf, const std::string& channel,
                            const Msg *msg, void *usr)
{
    std::function<void (const ReceiveBuffer *rbuf,
                        const std::string& channel,
                        const Msg *msg)>* cb = (std::function<void (const ReceiveBuffer *rbuf,
                                                                    const std::string& channel,
                                                                    const Msg *msg)>*) usr;
    (*cb)(rbuf, channel, msg);
}

template <class Msg>
inline Subscription *ZCM::subscribe(const std::string& channel,
                                    std::function<void (const ReceiveBuffer *rbuf,
                                                        const std::string& channel,
                                                        const Msg *msg)> cb)
{
    if (!zcm) {
        fprintf(stderr, "ZCM instance not initialized. Ignoring call to subscribe()\n");
        return nullptr;
    }

    typedef TypedSubscription<Msg> SubType;
    SubType *sub = new SubType();
    sub->usr = &cb;
    sub->typedCallback = __zcm__cbHelper;
    sub->c_sub = zcm_subscribe(zcm, channel.c_str(), SubType::dispatch, sub);

    subscriptions.push_back(sub);
    return sub;
}
#endif

inline Subscription *ZCM::subscribe(const std::string& channel,
                                    void (*cb)(const ReceiveBuffer *rbuf,
                                               const std::string& channel, void *usr),
                                    void *usr)
{
    if (!zcm) {
        fprintf(stderr, "ZCM instance not initialized.  Ignoring call to subscribe()\n");
        return nullptr;
    }

    typedef Subscription SubType;
    SubType *sub = new SubType();
    sub->usr = usr;
    sub->callback = cb;
    sub->c_sub = zcm_subscribe(zcm, channel.c_str(), SubType::dispatch, sub);

    subscriptions.push_back(sub);
    return sub;
}

inline void ZCM::unsubscribe(Subscription *sub)
{
    std::vector<Subscription*>::iterator end = subscriptions.end(),
                                          it = subscriptions.begin();
    for (; it != end; ++it) {
        if (*it == sub) {
            zcm_unsubscribe(zcm, sub->c_sub);
            subscriptions.erase(it);
            delete sub;
            break;
        }
    }
}

inline zcm_t *ZCM::getUnderlyingZCM()
{
    return zcm;
}

#ifndef ZCM_EMBEDDED
inline LogFile::LogFile(const std::string& path, const std::string& mode)
{
    this->eventlog = zcm_eventlog_create(path.c_str(), mode.c_str());
    this->lastevent = nullptr;
}

inline LogFile::~LogFile()
{
    if (eventlog)
        zcm_eventlog_destroy(eventlog);
    eventlog = nullptr;
    if(lastevent)
        zcm_eventlog_free_event(lastevent);
    lastevent = nullptr;
}

inline bool LogFile::good() const
{
    return eventlog != nullptr;
}

inline int LogFile::seekToTimestamp(int64_t timestamp)
{
    return zcm_eventlog_seek_to_timestamp(eventlog, timestamp);
}

inline FILE* LogFile::getFilePtr()
{
    return zcm_eventlog_get_fileptr(eventlog);
}

inline const LogEvent* LogFile::readNextEvent()
{
    zcm_eventlog_event_t* evt = zcm_eventlog_read_next_event(eventlog);
    if (lastevent)
        zcm_eventlog_free_event(lastevent);
    lastevent = evt;
    if (!evt)
        return nullptr;
    curEvent.eventnum = evt->eventnum;
    curEvent.channel.assign(evt->channel, evt->channellen);
    curEvent.timestamp = evt->timestamp;
    curEvent.datalen = evt->datalen;
    curEvent.data = (char*)evt->data;
    return &curEvent;
}

inline int LogFile::writeEvent(LogEvent* event)
{
    zcm_eventlog_event_t evt;
    evt.eventnum = event->eventnum;
    evt.timestamp = event->timestamp;
    evt.channellen = event->channel.size();
    evt.datalen = event->datalen;
    evt.channel = (char*) event->channel.c_str();
    evt.data = event->data;
    return zcm_eventlog_write_event(eventlog, &evt);
}
#endif
