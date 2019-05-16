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
typedef zcm_msg_handler_t MsgHandler;
class Subscription;

class ZCM
{
  public:
    #ifndef ZCM_EMBEDDED
     ZCM();
     ZCM(const std::string& transport);
    #endif
     ZCM(zcm_trans_t* zt);
    virtual  ~ZCM();

      bool good() const;
    virtual  int err() const; // errno is a reserved name, so this returns zcm_errno()
    virtual  const char* strerror() const;
    virtual  const char* strerrno(int err) const;

    #ifndef ZCM_EMBEDDED
      void run();
      void start();
      void stop();
      void pause();
      void resume();
      int  handle();
      void setQueueSize(uint32_t sz);
    #endif
    virtual  int  handleNonblock();
    virtual  void flush();

  public:
     int publish(const std::string& channel, const uint8_t* data, uint32_t len);

    // Note: if we make a publish binding that takes a const message reference, the compiler does
    //       not select the right version between the pointer and reference versions, so when the
    //       user intended to call the pointer version, the reference version is called and causes
    //       compile errors (turns the input into a double pointer). We have to choose one or the
    //       other for the api.
    template <class Msg>
     int publish(const std::string& channel, const Msg* msg);

    Subscription* subscribe(const std::string& channel,
                                   void (*cb)(const ReceiveBuffer* rbuf,
                                              const std::string& channel,
                                              void* usr),
                                   void* usr);

    template <class Msg, class Handler>
     Subscription* subscribe(const std::string& channel,
                                   void (Handler::*cb)(const ReceiveBuffer* rbuf,
                                                       const std::string& channel,
                                                       const Msg* msg),
                                   Handler* handler);

    template <class Handler>
     Subscription* subscribe(const std::string& channel,
                                   void (Handler::*cb)(const ReceiveBuffer* rbuf,
                                                       const std::string& channel),
                                   Handler* handler);

    template <class Msg>
    Subscription* subscribe(const std::string& channel,
                                   void (*cb)(const ReceiveBuffer* rbuf,
                                              const std::string& channel,
                                              const Msg* msg, void* usr),
                                   void* usr);

    #if __cplusplus > 199711L
    template <class Msg>
     Subscription* subscribe(const std::string& channel,
                                   std::function<void (const ReceiveBuffer* rbuf,
                                                       const std::string& channel,
                                                       const Msg* msg)> cb);
    #endif

     void unsubscribe(Subscription* sub);

    virtual  zcm_t* getUnderlyingZCM();

  protected:
    /**** Methods for inheritor override ****/
    virtual  int publishRaw(const std::string& channel, const uint8_t* data, uint32_t len);

    // Set the value of "rawSub" with your underlying subscription. "rawSub" will be passed
    // (by reference) into unsubscribeRaw when zcm->unsubscribe() is called on a cpp subscription
    virtual  void subscribeRaw(void*& rawSub, const std::string& channel,
                                     MsgHandler cb, void* usr);

    // Unsubscribes from a raw subscription. Effectively undoing the actions of subscribeRaw
    virtual  void unsubscribeRaw(void*& rawSub);

  private:
    zcm_t* zcm;
    int _err = ZCM_EOK;
    std::vector<Subscription*> subscriptions;
};

// New class required to allow the Handler callbacks and std::string channel names
class Subscription
{
    friend class ZCM;
    void* rawSub;

  protected:
    void* usr;
    void (*callback)(const ReceiveBuffer* rbuf, const std::string& channel, void* usr);

  public:
    virtual ~Subscription() {}

    void* getRawSub() const
    { return rawSub; }

     void dispatch(const ReceiveBuffer* rbuf, const std::string& channel)
    { (*callback)(rbuf, channel, usr); }

    static  void dispatch(const ReceiveBuffer* rbuf, const char* channel, void* usr)
    { ((Subscription*)usr)->dispatch(rbuf, channel); }
};

// TODO: why not use or inherit from the existing zcm data structures for the below

#ifndef ZCM_EMBEDDED
struct LogEvent
{
    int64_t     eventnum;
    int64_t     timestamp;
    std::string channel;
    int32_t     datalen;
    uint8_t*       data;
};

struct LogFile
{
    /**** Methods for ctor/dtor/check ****/
     LogFile(const std::string& path, const std::string& mode);
     ~LogFile();
     bool good() const;
     void close();

    /**** Methods general operations ****/
     int seekToTimestamp(int64_t timestamp);
     FILE* getFilePtr();

    /**** Methods for read/write ****/
    // NOTE: user should NOT hold-onto the returned ptr across successive calls
     const LogEvent* readNextEvent();
     const LogEvent* readPrevEvent();
     const LogEvent* readEventAtOffset(off_t offset);
     int             writeEvent(const LogEvent* event);

  private:
     const LogEvent* cplusplusIfyEvent(zcm_eventlog_event_t* le);
    LogEvent curEvent;
    zcm_eventlog_t* eventlog;
    zcm_eventlog_event_t* lastevent;
};
#endif

#define __zcm_cpp_impl_ok__
#include "zcm-cpp-impl.hpp"
#undef __zcm_cpp_impl_ok__

}
