#pragma once

#include <unistd.h>
#include <string>
#include <vector>

#include "zcm/zcm.h"
#include "zcm/eventlog.h"

namespace zcm {

struct Subscription;
struct ReceiveBuffer;

struct ZCM
{
    // TODO: update to match new url based zcm_create
    inline ZCM(const std::string& transport="ipc");
    inline ~ZCM();

    inline bool good() const;
    inline int handle();

    inline int publish(const std::string& channel, const char *data,
                       uint len);

    template <class Msg>
    inline int publish(const std::string& channel, const Msg *msg);

    template <class Msg, class Handler>
    Subscription *subscribe(const std::string& channel,
            void (Handler::*cb)(const ReceiveBuffer *rbuf, const std::string& channel, const Msg *msg),
            Handler *handler);

    template <class Handler>
    Subscription *subscribe(const std::string& channel,
                            void (Handler::*cb)(const ReceiveBuffer* rbuf, const std::string& channel),
                            Handler* handler);

    // TODO: add unsubscribe

    inline zcm_t* getUnderlyingZCM();

  private:
    zcm_t *zcm;
    std::vector<Subscription*> subscriptions;
};

struct ReceiveBuffer
{
    // TODO: is there a reason to have the ZCM pointer in here? It seems like it is always
    //       set to nullptr
    ZCM     *zcm;

    uint64_t utime;
    size_t   datalen;
    char    *data;
};

struct Subscription
{
    Subscription() {};
    virtual ~Subscription() {}
};

struct LogEvent
{
    int64_t     eventnum;
    std::string channel;

    uint64_t    utime;
    size_t      datalen;
    char       *data;
};

struct LogFile
{
    /**** Methods for ctor/dtor/check ****/
    inline LogFile(const std::string& path, const std::string& mode);
    inline ~LogFile();
    inline bool good() const;

    /**** Methods general operations ****/
    inline int seekToTimestamp(int64_t timestamp);
    inline FILE* getFilePtr();

    /**** Methods for read/write ****/
    // NOTE: user should NOT hold-onto the returned ptr across successive calls
    inline const LogEvent* readNextEvent();
    inline int writeEvent(LogEvent* event);

  private:
    LogEvent curEvent;
    zcm_eventlog_t* eventlog;
    zcm_eventlog_event_t* lastevent;
};

#define __zcm_cpp_impl_ok__
#include "zcm-cpp-impl.hpp"
#undef __zcm_cpp_impl_ok__

}
