#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <typeinfo>
#include <cxxabi.h>
#include <sys/time.h>
#include <stdarg.h>

#include <zcm/zcm-cpp.hpp>
#include <zcm/util/circular.hpp>

static bool ZCM_DEBUG_ENABLED = (NULL != getenv("ZCM_DEBUG"));
#define ZCM_DEBUG(...) \
    do { \
        if (ZCM_DEBUG_ENABLED) \
            __ZCM_PRINT_OBFUSCATE__(std::cout, "ZCM-DEBUG: ", __VA_ARGS__, '\n'); \
    } while(0)

template <typename T>
class MessageTracker
{
  public:
    typedef void (*callback)(const T* msg, void* usr);

  private:
    zcm::ZCM* zcmLocal = nullptr;

    std::atomic_bool done {false};

    Circular<T> *buf;
    std::mutex bufLock;
    std::condition_variable newMsg;
    zcm::Subscription *s;

    std::thread *thr = nullptr;
    callback onMsg;
    void* usr;

    uint64_t maxTimeErr_us;

    void callbackThreadFunc()
    {
        while (!done) {
            T* localMsg = get();
            if (done) return;
            onMsg(localMsg, usr);
        }
    }

    void handle(const zcm::ReceiveBuffer* rbuf, const std::string& chan, const T* _msg)
    {
        if (done)
            return;

        T* tmp = new T(*_msg);

        {
            std::unique_lock<std::mutex> lk(bufLock);

            if (buf->isFull())
                buf->removeFront();

            while (!buf->isEmpty()) {
                if (getMsgUtime(buf->front()) + maxTimeErr_us > getMsgUtime(_msg))
                    break;
                buf->removeFront();
            }

            buf->pushBack(tmp);
        }
        newMsg.notify_all();
    }

    MessageTracker() {}

  protected:
    virtual uint64_t getMsgUtime(const T* msg)
    {
        return hasUtime<T>::utime(msg);
    }

    virtual T* interpolate(uint64_t utimeTarget,
                           const T* A, uint64_t utimeA,
                           const T* B, uint64_t utimeB)
    {
        return utimeTarget - utimeA < utimeB - utimeTarget ? new T(*A) : new T(*B);
    }

  public:
    static const bool NONBLOCKING = false;
    static const bool BLOCKING = true;

    MessageTracker(zcm::ZCM* zcmLocal, std::string channel,
                   uint64_t maxTimeErr_us = 0.25e6, size_t maxMsgs = 1,
                   callback onMsg = nullptr, void* usr = nullptr)
        : zcmLocal(zcmLocal), maxTimeErr_us(maxTimeErr_us), onMsg(onMsg), usr(usr)
    {
        if (hasUtime<T>::present == true) {
            T tmp;
            std::string name = demangle(getType(tmp));
            ZCM_DEBUG("Message trackers using 'utime' field of zcmtype ", name);
        } else {
            T tmp;
            std::string name = demangle(getType(tmp));
            ZCM_DEBUG("Message trackers using local system receive utime for ",
                      "tracking zcmtype ", name);
        }
        buf = new Circular<T>(maxMsgs);
        if (onMsg != nullptr)
            thr = new std::thread(&MessageTracker<T>::callbackThreadFunc, this);
        s = zcmLocal->subscribe(channel, &MessageTracker<T>::handle, this);
    }

    virtual ~MessageTracker()
    {
        zcmLocal->unsubscribe(s);
        done = true;
        newMsg.notify_all();
        if (thr) {
            thr->join();
            delete thr;
        }
        while (!buf->isEmpty())
            buf->removeFront();
        delete buf;
    }

    // You must free the memory returned here
    virtual T* get(bool blocking = NONBLOCKING)
    {
        T* ret = nullptr;

        {
            std::unique_lock<std::mutex> lk(bufLock);
            if (blocking == BLOCKING) {
                if (buf->isEmpty())
                    newMsg.wait(lk, [&]{ return !buf->isEmpty() || done; });
                if (done) return nullptr;
            }
            if (!buf->isEmpty())
                ret = new T(*buf->back());
        }

        return ret;
    }

    virtual T* get(uint64_t utime, bool blocking = NONBLOCKING)
    {
        T *m0 = nullptr, *m1 = nullptr; // two poses bracketing the desired utime
        uint64_t m0Utime, m1Utime;

        {
            std::unique_lock<std::mutex> lk(bufLock);

            if (blocking == BLOCKING) {
                if (buf->isEmpty())
                    newMsg.wait(lk, [&]{ return !buf->isEmpty() || done; });
                if (done) return nullptr;
            }

            size_t size = buf->size();

            const T *_m0 = nullptr;
            const T *_m1 = nullptr;

            for (size_t i = 0; i < size; ++i) {
                const T* m = (*buf)[i];
                uint64_t mUtime = getMsgUtime(m);

                if (mUtime <= utime && (_m0 == nullptr || mUtime > m0Utime)) {
                    _m0 = m;
                    m0Utime = getMsgUtime(_m0);
                }

                if (mUtime >= utime && (_m1 == nullptr || mUtime < m1Utime)) {
                    _m1 = m;
                    m1Utime = getMsgUtime(_m1);
                }
            }

            if (_m0 != nullptr)
                m0 = new T(*_m0);

            if (_m1 != nullptr)
                m1 = new T(*_m1);

        }

        if (m0 && utime - m0Utime > maxTimeErr_us) {
            delete m0;
            m0 = nullptr;
        }

        if (m1 && m1Utime - utime > maxTimeErr_us) {
            delete m1;
            m1 = nullptr;
        }

        if (m0 && m1) {
            if (m0Utime == m1Utime) {
                delete m1;
                return m0;
            }

            T* elt = interpolate(utime, m0, m0Utime, m1, m1Utime);

            delete m0;
            delete m1;

            return elt;
        }

        if (m0) return m0;
        if (m1) return m1;

        return nullptr;
    }

  private:
    // *****************************************************************************
    // Insanely hacky trick to determine at compile time if a zcmtype has a
    // field called "utime"
    template <typename F> struct hasUtime {
        struct Fallback { void* utime; }; // introduce member name "utime"
        struct Derived : F, Fallback {};

        template <typename C, C> struct ChT;

        template <typename C>
        static uint64_t _utime(ChT<void* Fallback::*, &C::utime>*, const F* msg)
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
        };
        template <typename C>
        static uint64_t _utime(void*, ...)
        {
            va_list args;
            va_start(args, 1);
            const F* msg = va_arg(args, const F*);
            va_end(args);
            return msg->utime;
        }

        template<typename C> static char (&f(ChT<int Fallback::*, &C::x>*))[1];
        template<typename C> static char (&f(...))[2];

        static bool const present = sizeof(f<Derived>(0)) == 2;

        static uint64_t const utime(const F* msg)
        {
            return _utime<Derived>(0,msg);
        }
    };

    template<typename F>
    static inline std::string getType(const F t) { return typeid(t).name(); }

    static inline std::string demangle(std::string name)
    {
        int status = -4; // some arbitrary value to eliminate the compiler warning
        std::unique_ptr<char, void(*)(void*)> res {
            abi::__cxa_demangle(name.c_str(), NULL, NULL, &status),
            std::free
        };
        return (status==0) ? res.get() : name ;
    }

    static void __ZCM_PRINT_OBFUSCATE__(std::ostream& o) { }

    template<typename First, typename ...Rest>
    static void __ZCM_PRINT_OBFUSCATE__(std::ostream& o, First && first, Rest && ...rest)
    {
        o << std::forward<First>(first);
        __ZCM_PRINT_OBFUSCATE__(o, std::forward<Rest>(rest)...);
    }
    // *****************************************************************************

};

#undef ZCM_DEBUG
