#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "zcm/zcm.h"

#ifndef ZCM_EMBEDDED
#include "zcm/eventlog.h"
#endif

#if __cplusplus > 199711L
#include <functional>
#endif

namespace zcm {

typedef zcm_recv_buf_t ReceiveBuffer;
class Subscription;

// TODO: unify pointer style pref "Msg* msg" vs "Msg *msg", I'd tend toward the former

class ZCM
{
  public:
    inline ZCM();
    inline ZCM(const std::string& transport);
    inline ZCM(zcm_trans_t *zt);
    inline ~ZCM();

    inline bool good() const;

    inline int err(); // get the latest zcm err code
    inline const char *strerror();

    inline void run();
    inline void start();
    inline void stop();
    inline int handle();
    inline int handleNonblock();

    inline void flush();

    inline int publish(const std::string& channel, const char *data, uint32_t len);

    // Note: if we make a publish binding that takes a const message reference, the compiler does
    //       not select the right version between the pointer and reference versions, so when the
    //       user intended to call the pointer version, the reference version is called and causes
    //       compile errors (turns the input into a double pointer). We have to choose one or the
    //       other for the api.
    template <class Msg>
    inline int publish(const std::string& channel, const Msg *msg);

    template <class Msg, class Handler>
    inline Subscription *subscribe(const std::string& channel,
                                   void (Handler::*cb)(const ReceiveBuffer *rbuf,
                                                       const std::string& channel, const Msg *msg),
                                   Handler *handler);

    template <class Handler>
    inline Subscription *subscribe(const std::string& channel,
                                   void (Handler::*cb)(const ReceiveBuffer* rbuf,
                                                       const std::string& channel),
                                   Handler* handler);

    template <class Msg>
    inline Subscription *subscribe(const std::string& channel,
                                   void (*cb)(const ReceiveBuffer *rbuf, const std::string& channel,
                                              const Msg *msg, void *usr),
                                   void *usr);

    #if __cplusplus > 199711L
    template <class Msg>
    inline Subscription *subscribe(const std::string& channel,
                                   std::function<void (const ReceiveBuffer *rbuf,
                                                       const std::string& channel,
                                                       const Msg *msg)> cb);
    #endif

    inline Subscription *subscribe(const std::string& channel,
                                   void (*cb)(const ReceiveBuffer *rbuf, const std::string& channel,
                                              void *usr),
                                   void *usr);

    inline void unsubscribe(Subscription *sub);

    inline zcm_t* getUnderlyingZCM();

  private:
    zcm_t *zcm;
    std::vector<Subscription*> subscriptions;
};

// New class required to allow the Handler callbacks and std::string channel names
class Subscription
{
    friend class ZCM;
    zcm_sub_t *c_sub;

  protected:
    void *usr;
    void (*callback)(const ReceiveBuffer* rbuf, const std::string& channel, void *usr);

  public:
    virtual ~Subscription() {}

    inline void dispatch(const ReceiveBuffer *rbuf, const char *channel)
    {
        (*callback)(rbuf, channel, usr);
    }

    static inline void dispatch(const ReceiveBuffer *rbuf, const char *channel, void *usr)
    {
        ((Subscription*)usr)->dispatch(rbuf, channel);
    }
};

// TODO: why not use or inherit from the existing zcm data structures for the below

#ifndef ZCM_EMBEDDED
struct LogEvent
{
    int64_t     eventnum;
    int64_t     timestamp;
    std::string channel;
    int32_t     datalen;
    char*       data;
};

struct LogFile
{
    /**** Methods for ctor/dtor/check ****/
    inline LogFile(const std::string& path, const std::string& mode);
    inline ~LogFile();
    inline bool good() const;
    inline void close();

    /**** Methods general operations ****/
    inline int seekToTimestamp(int64_t timestamp);
    inline FILE* getFilePtr();

    /**** Methods for read/write ****/
    // NOTE: user should NOT hold-onto the returned ptr across successive calls
    inline const LogEvent* readNextEvent();
    inline const LogEvent* readPrevEvent();
    inline const LogEvent* readEventAtOffset(off_t offset);
    inline int             writeEvent(const LogEvent* event);

  private:
    inline const LogEvent* cplusplusIfyEvent(zcm_eventlog_event_t* le);
    LogEvent curEvent;
    zcm_eventlog_t* eventlog;
    zcm_eventlog_event_t* lastevent;
};
#endif

#define __zcm_cpp_impl_ok__
#include "zcm-cpp-impl.hpp"
#undef __zcm_cpp_impl_ok__

}
