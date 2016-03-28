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

// RRR: kinda applies to a lot of this repo, but I wish we named the files with
//      the approapriate capitalization (ie exactly like the class names) but I
//      realize that it's a repo-wide problem, not just this file

static bool __ZCM_DEBUG_ENABLED__ = (NULL != getenv("ZCM_DEBUG"));
#define ZCM_DEBUG(...) \
    do { \
        if (__ZCM_DEBUG_ENABLED__) \
            __ZCM_PRINT_OBFUSCATE__(std::cout, "ZCM-DEBUG: ", __VA_ARGS__, '\n'); \
    } while(0)

namespace zcm {

template <typename T>
class MessageTracker
{
  public:
    typedef void (*callback)(T* msg, uint64_t utime, void* usr);
    static const bool NONBLOCKING = false;
    static const bool BLOCKING = true;

  protected:
    virtual uint64_t getMsgUtime(const T* msg)
    {
        return hasUtime<T>::utime(msg);
    }

    // RRR: note that the returned value must be "new" in all cases
    virtual T* interpolate(uint64_t utimeTarget,
                           const T* A, uint64_t utimeA,
                           const T* B, uint64_t utimeB)
    {
        return utimeTarget - utimeA < utimeB - utimeTarget ? new T(*A) : new T(*B);
    }

  private:
    zcm::ZCM* zcmLocal = nullptr;

    std::atomic_bool done {false};

    Circular<T> *buf;
    std::mutex bufLock;
    std::condition_variable newMsg;
    zcm::Subscription *s;

    std::mutex callbackLock;
    std::condition_variable callbackCv;
    T* callbackMsg = nullptr;
    std::thread *thr = nullptr;
    callback onMsg;
    void* usr;

    uint64_t maxTimeErr_us;

    void callbackThreadFunc()
    {
        std::unique_lock<std::mutex> lk(callbackLock);
        while (!done) {
            callbackCv.wait(lk, [&](){ return callbackMsg || done; });
            if (done) return;
            onMsg(callbackMsg, getMsgUtime(callbackMsg), usr);
            // Intentionally not deleting callbackMsg as it is the
            // responsibility of the callback to delete the memory
            callbackMsg = nullptr;
        }
    }

    void handle(const zcm::ReceiveBuffer* rbuf, const std::string& chan, const T* _msg)
    {
        if (done) return;

        T* tmp = new T(*_msg);

        {
            std::unique_lock<std::mutex> lk(bufLock);

            if (buf->isFull()) buf->removeFront();

            while (!buf->isEmpty()) {
                if (getMsgUtime(buf->front()) + maxTimeErr_us > getMsgUtime(_msg)) break;
                buf->removeFront();
            }

            buf->pushBack(tmp);
        }
        newMsg.notify_all();

        // RRR: might be interesting to have a derived version of this class with a set
        //      number of buffered callbackMsg's but definitely not required for this one
        if (callbackLock.try_lock()) {
            if (callbackMsg) delete callbackMsg;
            callbackMsg = new T(*_msg);
            callbackLock.unlock();
            callbackCv.notify_all();
        }
    }

    MessageTracker() {}

  public:
    MessageTracker(zcm::ZCM* zcmLocal, std::string channel,
                   double maxTimeErr = 0.25, size_t maxMsgs = 1,
                   callback onMsg = nullptr, void* usr = nullptr)
        : zcmLocal(zcmLocal), maxTimeErr_us(maxTimeErr * 1e6), onMsg(onMsg), usr(usr)
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

        // RRR: technically you don't need to use dynamic memory for the thread, you can see
        //      an example of doing it the other way in kestrel/src/cpp-c/util/ThreadUtil.hpp
        //      (basically thread has a move constructor and you just check for thr.joinable()
        //      before calling .join())
        if (onMsg != nullptr) thr = new std::thread(&MessageTracker<T>::callbackThreadFunc, this);
        s = zcmLocal->subscribe(channel, &MessageTracker<T>::handle, this);
    }

    virtual ~MessageTracker()
    {
        zcmLocal->unsubscribe(s);
        done = true;
        newMsg.notify_all();
        callbackCv.notify_all();
        if (thr) {
            thr->join();
            delete thr;
        }
        while (!buf->isEmpty()) buf->removeFront();
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

    // This may return nullptr even in the blocking case
    virtual T* get(uint64_t utime, bool blocking = NONBLOCKING)
    {
        T *m0 = nullptr, *m1 = nullptr; // two poses bracketing the desired utime
        uint64_t m0Utime, m1Utime;

        {
            std::unique_lock<std::mutex> lk(bufLock);

            if (blocking == BLOCKING) {
                // RRR: I think I know what you are talking about, but this comment isn't
                //      very clearly worded
                // XXX This is technically not okay. "done" can't be an
                //     atomic as it can change after we've checked it in the
                //     wait. If it does change, and we go back to sleep,
                //     we'll never wake up
                newMsg.wait(lk, [&]{
                            if (done) return true;
                            if (buf->isEmpty()) return false;
                            uint64_t recentUtime = getMsgUtime(buf->back());
                            // RRR: there should probably be an epsilon on this
                            if (recentUtime < utime) return false;
                            return true;
                        });
                if (done) return nullptr;
            }

            size_t size = buf->size();

            const T *_m0 = nullptr;
            const T *_m1 = nullptr;

            // RRR: I guess if you aren't assuming ordering of the messages based on utime
            //      this is necessary, but you could search faster if you assumed (or enforced)
            //      an ordering
            for (size_t i = 0; i < size; ++i) {
                const T* m = (*buf)[i];
                uint64_t mUtime = getMsgUtime(m);

                if (mUtime <= utime && (_m0 == nullptr || mUtime > m0Utime)) {
                    _m0 = m;
                    // RRR: here getMsgUtime(_m0) == mUtime no?
                    m0Utime = getMsgUtime(_m0);
                }

                if (mUtime >= utime && (_m1 == nullptr || mUtime < m1Utime)) {
                    _m1 = m;
                    // RRR: here getMsgUtime(_m1) == mUtime no?
                    m1Utime = getMsgUtime(_m1);
                }
            }

            if (_m0 != nullptr) m0 = new T(*_m0);
            if (_m1 != nullptr) m1 = new T(*_m1);

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
    // TODO: I think c++11 has some utilities to make this a bit easier (I have not
    //       looked into them much at all, but worth looking at <type_traits>
    //       and what you can do with things like std::true_type and the like)
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

}

#undef ZCM_DEBUG
