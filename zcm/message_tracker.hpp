#pragma once

#include <iostream>
#include <cstdint>
#include <string>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <typeinfo>
#include <cxxabi.h>
#include <sys/time.h>
#include <stdarg.h>

#include <zcm/zcm-cpp.hpp>
#include <zcm/util/Filter.hpp>

static bool __ZCM_DEBUG_ENABLED__ = (NULL != getenv("ZCM_DEBUG"));
#define ZCM_DEBUG(...) \
    do { \
        if (__ZCM_DEBUG_ENABLED__) \
            __ZCM_PRINT_OBFUSCATE__(std::cout, "ZCM-DEBUG: ", __VA_ARGS__, '\n'); \
    } while(0)

namespace zcm {

template <typename T>
class Tracker
{
  public:
    // You must free the memory passed into this callback
    typedef std::function<void (T* msg, uint64_t utime, void* usr)> callback;
    static const bool NONBLOCKING = false;
    static const bool BLOCKING = true;

  protected:
    virtual uint64_t getMsgUtime(const T* msg)
    {
        return hasUtime<T>::utime(msg);
    }

    // The returned value must be "new" in all cases
    virtual T* interpolate(uint64_t utimeTarget,
                           const T* A, uint64_t utimeA,
                           const T* B, uint64_t utimeB)
    {
        return utimeTarget - utimeA < utimeB - utimeTarget ? new T(*A) : new T(*B);
    }

  private:
    uint64_t maxTimeErr_us;

    std::atomic_bool done {false};

    std::deque<T*> buf;
    uint64_t lastHostUtime = UINT64_MAX;
    size_t bufMax;
    std::mutex bufLock;
    std::condition_variable newMsgCond;

    std::mutex callbackLock;
    std::condition_variable callbackCv;
    T* callbackMsg = nullptr;
    std::thread *thr = nullptr;
    callback onMsg;
    void* usr;

    Filter hzFilter;
    Filter jitterFilter;

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

    Tracker() {}

  public:
    Tracker(double maxTimeErr = 0.25, size_t maxMsgs = 1,
            callback onMsg = callback(), void* usr = nullptr,
            double freqEstConvergenceNumMsgs = 10)
        : maxTimeErr_us(maxTimeErr * 1e6), onMsg(onMsg), usr(usr),
              hzFilter(Filter::convergenceTimeToNatFreq(freqEstConvergenceNumMsgs, 0.8), 0.8),
          jitterFilter(Filter::convergenceTimeToNatFreq(freqEstConvergenceNumMsgs, 1), 1)
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

        bufMax = maxMsgs;

        if (onMsg) thr = new std::thread(&Tracker<T>::callbackThreadFunc, this);
    }

    virtual ~Tracker()
    {
        done = true;
        newMsgCond.notify_all();
        callbackCv.notify_all();
        if (callbackMsg) delete callbackMsg;
        if (thr) {
            thr->join();
            delete thr;
        }
        while (!buf.empty()) {
            T* tmp = buf.front();
            delete tmp;
            buf.pop_front();
        }
    }

    // You must free the memory returned here
    T* get(bool blocking = NONBLOCKING)
    {
        T* ret = nullptr;

        {
            std::unique_lock<std::mutex> lk(bufLock);
            if (blocking == BLOCKING) {
                if (buf.empty())
                    newMsgCond.wait(lk, [&]{ return !buf.empty() || done; });
                if (done) return nullptr;
            }
            if (!buf.empty())
                ret = new T(*buf.back());
        }

        return ret;
    }

    // This may return nullptr even in the blocking case
    // TODO: Should consider how to allow the user to ask for an extrapolated
    //       message if bracketted messages arent available
    T* get(uint64_t utime, bool blocking = NONBLOCKING)
    {
        std::unique_lock<std::mutex> lk(bufLock);

        if (blocking == BLOCKING) {
            // XXX This is technically not okay. "done" can't be an
            //     atomic as it can change after we've checked it in the
            //     wait. If it does change, and we go back to sleep,
            //     we'll never wake up. In other words, if done changes
            //     to true between the "if (done) return false;" and the
            //     end of the wait function, we would go back to sleep and
            //     never wake up again because this class would have already
            //     cleaned up
            newMsgCond.wait(lk, [&]{
                        if (done) return true;
                        if (buf.empty()) return false;
                        uint64_t recentUtime = getMsgUtime(buf.back());
                        if (recentUtime < utime) return false;
                        return true;
                    });
            if (done) return nullptr;
        }

        return get(utime, buf.begin(), buf.end(), lk);
    }

    // If you need to have a lock while working with the iterators, pass it in
    // here to have it unlocked once this function is done working with the iterators
    template <class InputIter>
    T* get(uint64_t utime, InputIter first, InputIter last, std::unique_lock<std::mutex>& lk)
    {
        T *m0 = nullptr, *m1 = nullptr; // two poses bracketing the desired utime
        uint64_t m0Utime = 0, m1Utime = UINT64_MAX;

        const T *_m0 = nullptr;
        const T *_m1 = nullptr;

        // The reason why we do a linear search is to support the ability
        // to skip around in logs. We want to support messages with
        // non-monitonically increasing utimes and this is the easiest way.
        // This can be made much faster if needed
        for (auto iter = first; iter != last; iter++) {
            const T* m = *iter;
            uint64_t mUtime = getMsgUtime(m);

            if (mUtime <= utime && (_m0 == nullptr || mUtime > m0Utime)) {
                _m0 = m;
                m0Utime = mUtime;
            }

            if (mUtime >= utime && (_m1 == nullptr || mUtime < m1Utime)) {
                _m1 = m;
                m1Utime = mUtime;
            }
        }

        if (_m0 != nullptr) m0 = new T(*_m0);
        if (_m1 != nullptr) m1 = new T(*_m1);

        if (lk.owns_lock()) lk.unlock();

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

    std::vector<T*> getRange(uint64_t utimeA, uint64_t utimeB, bool blocking = NONBLOCKING)
    {
        std::unique_lock<std::mutex> lk(bufLock);

        if (blocking == BLOCKING) {
            // XXX This is technically not okay. "done" can't be an
            //     atomic as it can change after we've checked it in the
            //     wait. If it does change, and we go back to sleep,
            //     we'll never wake up. In other words, if done changes
            //     to true between the "if (done) return false;" and the
            //     end of the wait function, we would go back to sleep and
            //     never wake up again because this class would have already
            //     cleaned up
            newMsgCond.wait(lk, [&]{
                        if (done) return true;
                        if (buf.empty()) return false;
                        uint64_t recentUtime = getMsgUtime(buf.back());
                        if (recentUtime < utimeA) return false;
                        return true;
                    });
            if (done) return std::vector<T*>();
        }

        uint64_t m0Utime = 0, m1Utime = UINT64_MAX;

        for (const T* m : buf) {
            uint64_t mUtime = getMsgUtime(m);
            if (mUtime <= utimeA && mUtime > m0Utime) m0Utime = mUtime;
            if (mUtime >= utimeB && mUtime < m1Utime) m1Utime = mUtime;
        }

        std::vector<T*> ret;
        for (const T* m : buf) {
            uint64_t mUtime = getMsgUtime(m);
            if (mUtime >= m0Utime && mUtime <= m1Utime) ret.push_back(new T(*m));
        }
        return ret;
    }

    void newMsg(const T* _msg, uint64_t hostUtime = UINT64_MAX)
    {
        if (done) return;

        T* tmp = new T(*_msg);
        {
            std::unique_lock<std::mutex> lk(bufLock);

            // Expire due to buffer being full
            if (buf.size() == bufMax){
                T* tmp = buf.front();
                delete tmp;
                buf.pop_front();
            }

            // Expire things that are too old
            while (!buf.empty()) {
                if (getMsgUtime(buf.front()) + maxTimeErr_us > getMsgUtime(_msg)) break;
                T* tmp = buf.front();
                delete tmp;
                buf.pop_front();
            }

            if (lastHostUtime != UINT64_MAX) {
                double obs = hostUtime - lastHostUtime;
                hzFilter(obs, 1);
                double jitterObs = obs - hzFilter[Filter::LOW_PASS];
                double jitterObsSq = jitterObs * jitterObs;
                jitterFilter(jitterObsSq, 1);
                /*
                std::cout << hostUtime << ", "
                          << jitterObs << ", "
                          << sqrt(jitterFilter.lowPass()) << ", "
                          << obs << ", "
                          << hzFilter.lowPass()
                          << std::endl;
                // */
            }

            lastHostUtime = hostUtime;
            buf.push_back(tmp);
        }
        newMsgCond.notify_all();

        if (thr) {
            if (callbackLock.try_lock()) {
                if (callbackMsg) delete callbackMsg;
                callbackMsg = new T(*_msg);
                callbackLock.unlock();
                callbackCv.notify_all();
            }
        }
    }

    double getHz()
    {
        std::unique_lock<std::mutex> lk(bufLock);
        return 1e6 / hzFilter[Filter::LOW_PASS];
    }

    uint64_t lastMsgHostUtime(bool blocking = NONBLOCKING)
    {
        {
            std::unique_lock<std::mutex> lk(bufLock);
            if (blocking == BLOCKING) {
                if (buf.empty())
                    newMsgCond.wait(lk, [&]{ return !buf.empty() || done; });
                if (done) return UINT64_MAX;
            }
            if (!buf.empty())
                return lastHostUtime;
        }
        return UINT64_MAX;
    }

    double getJitterUs()
    {
        std::unique_lock<std::mutex> lk(bufLock);
        return sqrt(jitterFilter[Filter::LOW_PASS]);
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
        static uint64_t _utime(void* tmp, ...)
        {
            va_list args;
            va_start(args, tmp);
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

template <typename T>
class MessageTracker : public Tracker<T>
{
  private:
    zcm::ZCM* zcmLocal = nullptr;
    zcm::Subscription *s = nullptr;

    void handle(const zcm::ReceiveBuffer* rbuf, const std::string& chan, const T* _msg)
    {
        Tracker<T>::newMsg(_msg, rbuf->recv_utime);
    }

    MessageTracker() {}

  public:
    MessageTracker(zcm::ZCM* zcmLocal, std::string channel,
                   double maxTimeErr = 0.25, size_t maxMsgs = 1,
                   typename Tracker<T>::callback onMsg = typename Tracker<T>::callback(),
                   void* usr = nullptr,
                   double freqEstConvergenceNumMsgs = 20)
        : Tracker<T>(maxTimeErr, maxMsgs, onMsg, usr, freqEstConvergenceNumMsgs),
          zcmLocal(zcmLocal)
    {
        if (zcmLocal && channel != "")
            s = zcmLocal->subscribe(channel, &MessageTracker<T>::handle, this);
    }

    virtual ~MessageTracker()
    {
        if (s) zcmLocal->unsubscribe(s);
    }
};

}

#undef ZCM_DEBUG
