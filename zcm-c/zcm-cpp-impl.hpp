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
class TypedSubStub : public Subscription
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
            nullptr,
            rbuf->utime,
            rbuf->len,
            rbuf->data,
        };
        std::string chan_str(channel);
        (handler->*cb)(&rb, chan_str, &msgMem);
    }

    // A standard vanilla function to bind the this-pointer
    static void dispatch_(const zcm_recv_buf_t *rbuf, const char *channel,
                          void *usr)
    {
        typedef TypedSubStub<Msg, Handler> MyType;
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
    typedef TypedSubStub<Msg, Handler> StubType;
    StubType *stub = new StubType();
    stub->handler = handler;
    stub->cb = cb;
    zcm_subscribe(zcm, channel.c_str(), StubType::dispatch_, stub);

    subscriptions.push_back(stub);
    return stub;
}

template <class Handler>
class UntypedSubStub : public Subscription
{
    friend class ZCM;
  private:
    Handler* handler;
    void (Handler::*cb)(const ReceiveBuffer* rbuf, const std::string& channel);

    void dispatch(const zcm_recv_buf_t *rbuf, const char *channel)
    {
        const ReceiveBuffer rb = {
            nullptr,
            rbuf->utime,
            rbuf->len,
            rbuf->data,
        };
        std::string chan_str(channel);
        (handler->*cb)(&rb, chan_str);
    }

    // A standard vanilla function to bind the this-pointer
    static void dispatch_(const zcm_recv_buf_t *rbuf, const char *channel,
                          void *usr)
    {
        typedef UntypedSubStub<Handler> MyType;
        ((MyType*)usr)->dispatch(rbuf, channel);
    }
};

template <class Handler>
Subscription *ZCM::subscribe(const std::string& channel,
                             void (Handler::*cb)(const ReceiveBuffer* rbuf, const std::string& channel),
                             Handler* handler)
{
    if (!zcm) {
        fprintf(stderr,
                "ZCM instance not initialized.  Ignoring call to subscribe()\n");
        return nullptr;
    }
    typedef UntypedSubStub<Handler> StubType;
    StubType *stub = new StubType();
    stub->handler = handler;
    stub->cb = cb;
    zcm_subscribe(zcm, channel.c_str(), StubType::dispatch_, stub);

    subscriptions.push_back(stub);
    return stub;
}

inline zcm_t *ZCM::getUnderlyingZCM()
{
    return zcm;
}

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
    curEvent.utime = evt->timestamp;
    curEvent.datalen = evt->datalen;
    curEvent.data = (char*)evt->data;
    return &curEvent;
}

inline int LogFile::writeEvent(LogEvent* event)
{
    zcm_eventlog_event_t evt;
    evt.eventnum = event->eventnum;
    evt.timestamp = event->utime;
    evt.channellen = event->channel.size();
    evt.datalen = event->datalen;
    evt.channel = (char*) event->channel.c_str();
    evt.data = event->data;
    return zcm_eventlog_write_event(eventlog, &evt);
}
