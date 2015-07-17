#pragma once

#include <unistd.h>
#include <string>
#include <vector>
#include "zcm.h"

namespace zcm {

struct Subscription;
struct ReceiveBuffer;

struct ZCM
{
    inline ZCM();
    inline ~ZCM();

    inline bool good() const;

    // XXX this is wrong
    inline int handle()
    {
        while (1)
            usleep(10000000);
    }

    inline int publish(const std::string& channel, const char *data,
                       uint len);

    template <class Msg>
    inline int publish(const std::string& channel, const Msg *msg);

    template <class Msg, class Handler>
    Subscription *subscribe(const std::string& channel,
            void (Handler::*cb)(const ReceiveBuffer *rbuf, const std::string& channel, const Msg *msg),
            Handler *handler);

    // template <class Handler>
    // Subscription *subscribe(const std::string& channel,
    //                         void (Handler::*cb)(const ReceiveBuffer* rbuf, const std::string& channel),
    //                         Handler* handler);

    inline zcm_t* getUnderlyingZCM();

  private:
    zcm_t *zcm;
    std::vector<Subscription*> subscriptions;
};

struct ReceiveBuffer
{
    char *data;
    size_t len;
    uint64_t utime;
    ZCM *zcm;
};

struct Subscription
{
    Subscription() {};
    virtual ~Subscription() {}
};

#define __zcm_cpp_impl_ok__
#include "zcm-cpp-impl.hpp"
#undef __zcm_cpp_impl_ok__

}
