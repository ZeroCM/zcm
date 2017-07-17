#pragma once

#include <iostream>
#include <cmath>
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
#include <functional>
#include <tuple>
#include <type_traits>

#include <zcm/zcm-cpp.hpp>
#include <zcm/util/Filter.hpp>

static bool __ZCM_DEBUG_ENABLED__ = (NULL != getenv("ZCM_DEBUG"));
#define ZCM_DEBUG(...) \
    do { \
        if (__ZCM_DEBUG_ENABLED__) \
            __ZCM_PRINT_OBFUSCATE__(std::cout, "ZCM-DEBUG: ", __VA_ARGS__, '\n'); \
    } while(0)

// Forward declaration of the test
class MessageTrackerTest;

namespace zcm {

template <typename T>
class Tracker
{
  public:
    // You must free the memory passed into this callback
    // This callback is not guaranteed to be called on every message.
    // It will only be called again on a new message that is received *after*
    // the last call to this callback has returned.
    typedef std::function<void (T* msg, uint64_t utime, void* usr)> callback;

    typedef T ZcmType;

  protected:
    virtual uint64_t getMsgUtime(const T* msg) const { return UINT64_MAX; }

    // The returned value must be "new" in all cases
    virtual T* interpolate(uint64_t utimeTarget,
                           const T* A, uint64_t utimeA,
                           const T* B, uint64_t utimeB) const
    {
        return utimeTarget - utimeA < utimeB - utimeTarget ? new T(*A) : new T(*B);
    }

  private:
    // *****************************************************************************
    // Insanely hacky trick to determine at compile time if a zcmtype has a
    // field called "utime"
    // TODO: I think c++11 has some utilities to make this a bit easier (I have not
    //       looked into them much at all, but worth looking at <type_traits>
    //       and what you can do with things like std::true_type and the like)
    template <typename F> struct hasUtime {
        struct Fallback { int utime; }; // introduce member name "utime"
        struct Derived : F, Fallback {};

        template <typename C, C> struct ChT;

        template<typename C> static char (&f(ChT<int Fallback::*, &C::utime>*))[1];
        template<typename C> static char (&f(...))[2];

        static constexpr bool present = sizeof(f<Derived>(0)) == 2;
    };

    // Continuing the craziness. Template specialization allows for storing a
    // host utime with a msg if there is no msg.utime field
    template<typename F, bool>
    struct MsgWithUtime;

    template<typename F>
    struct MsgWithUtime<F, true> : public F {
        MsgWithUtime(const F& msg, uint64_t utime) : F(msg) {}
        MsgWithUtime(const MsgWithUtime& msg) : F(msg) {}
        virtual ~MsgWithUtime() {}
    };

    template<typename F>
    struct MsgWithUtime<F, false> : public F {
        uint64_t utime;
        MsgWithUtime(const F& msg, uint64_t utime) : F(msg), utime(utime) {}
        MsgWithUtime(const MsgWithUtime& msg) : F(msg), utime(msg.utime) {}
        virtual ~MsgWithUtime() {}
    };

    typedef MsgWithUtime<T, hasUtime<T>::present> MsgType;

    // *****************************************************************************

    uint64_t maxTimeErr_us;

    // This is only to be used for the callback thread func
    bool done = false;


    std::deque<MsgType*> buf;
    uint64_t lastHostUtime = UINT64_MAX;
    size_t bufMax;
    // RRR: just use a typedef since you need to use the lock type in multiple places in file
    mutable std::recursive_mutex bufLock;

    decltype(bufLock) callbackLock;
    std::condition_variable_any callbackCv;
    MsgType* callbackMsg = nullptr;
    std::thread *thr = nullptr;
    callback onMsg;
    void* usr;

    Filter hzFilter;
    Filter jitterFilter;

    void callbackThreadFunc()
    {
        std::unique_lock<decltype(bufLock)> lk(callbackLock);
        while (!done) {
            callbackCv.wait(lk, [&](){ return callbackMsg || done; });
            if (done) return;
            onMsg(callbackMsg, callbackMsg->utime, usr);
            // Intentionally not deleting callbackMsg as it is the
            // responsibility of the callback to delete the memory
            callbackMsg = nullptr;
        }
    }

  public:
    // RRR: instead of using decltype to make your typedefs, make the typedef first and then
    //      declare the variable as that type
    // RRR: seems like this might be better named "ContainerType". To me RawType would
    //      indicate the type == T
    typedef decltype(buf) RawType;

    ///////////////////////////////
    //// Iterator Defn and Ops ////
    ///////////////////////////////

    typedef typename RawType::        const_iterator         const_iterator;
    typedef typename RawType::const_reverse_iterator const_reverse_iterator;

    inline          const_iterator cbegin() const { return buf. cbegin(); }
    inline          const_iterator   cend() const { return buf.   cend(); }

    inline const_reverse_iterator crbegin() const { return buf.crbegin(); }
    inline const_reverse_iterator   crend() const { return buf.  crend(); }

    uint64_t getMsgUtime(const MsgType* msg) const
    {
        uint64_t tmp = getMsgUtime((const T*)msg);
        if (tmp != UINT64_MAX) return tmp;
        return msg->utime;
    }

    Tracker(double maxTimeErr = 0.25, size_t maxMsgs = 1,
            callback onMsg = callback(), void* usr = nullptr,
            double freqEstConvergenceNumMsgs = 10)
        : maxTimeErr_us(maxTimeErr * 1e6), onMsg(onMsg), usr(usr),
              hzFilter(Filter::convergenceTimeToNatFreq(freqEstConvergenceNumMsgs, 0.8), 0.8),
          jitterFilter(Filter::convergenceTimeToNatFreq(freqEstConvergenceNumMsgs, 1), 1)
    {
        T tmp;
        std::string name = demangle(getType(tmp));

        if (hasUtime<T>::present == true) {
            ZCM_DEBUG("Message trackers using 'utime' field of zcmtype ", name);
        } else {
            ZCM_DEBUG("Message trackers using local system receive utime for ",
                      "tracking zcmtype ", name);
        }

        bufMax = maxMsgs;
        assert(maxMsgs > 0 && "Cannot allocate a tracker to track 0 messages");

        if (onMsg) thr = new std::thread(&Tracker<T>::callbackThreadFunc, this);
    }

    virtual ~Tracker()
    {
        if (thr) {
            {
                std::unique_lock<decltype(bufLock)> lk(callbackLock);
                done = true;
                callbackCv.notify_all();
            }
            thr->join();
            delete thr;
            if (callbackMsg) delete callbackMsg;
        }

        std::unique_lock<decltype(bufLock)> lk(bufLock);
        while (!buf.empty()) {
            MsgType* tmp = buf.front();
            delete tmp;
            buf.pop_front();
        }
    }

    // You must free the memory returned here. This may return nullptr
    T* get() const
    {
        T* ret = nullptr;

        {
            std::unique_lock<decltype(bufLock)> lk(bufLock);
            if (!buf.empty()) ret = new T(*buf.back());
        }

        return ret;
    }

    // Same semantics as get()
    T* get(uint64_t utime) const
    {
        std::unique_lock<decltype(bufLock)> lk(bufLock);
        return get(utime, buf.begin(), buf.end(), &lk);
    }

    // TODO: Should consider how to allow the user to ask for an extrapolated
    //       message if bracketted messages arent available
    // If you need to have a lock while working with the iterators, pass it in
    // here to have it unlocked once this function is done working with the iterators
    template <class InputIter, typename lockType = std::mutex>
    T* get(uint64_t utime, InputIter first, InputIter last,
           std::unique_lock<lockType>* lk = nullptr) const
    {
        static_assert(hasUtime<typename std::remove_pointer<typename
                      std::iterator_traits<InputIter>::value_type>::type>::present,
                      "Cannot call get with iterators that contain types without a utime field");

        MsgType *m0 = nullptr, *m1 = nullptr; // two poses bracketing the desired utime
        uint64_t m0Utime = 0, m1Utime = UINT64_MAX;

        const MsgType* _m0 = nullptr;
        const MsgType* _m1 = nullptr;

        // The reason why we do a linear search is to support the ability
        // to skip around in logs. We want to support messages with
        // non-monitonically increasing utimes and this is the easiest way.
        // This can be made much faster if needed
        for (auto iter = first; iter != last; ++iter) {
            // Note: This is unsafe unless we rely on the static assert at the beginning of
            //       the function
            const MsgType* m = (const MsgType*) *iter;
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

        if (_m0 != nullptr) m0 = new MsgType(*_m0);
        if (_m1 != nullptr) m1 = new MsgType(*_m1);

        if (lk && lk->owns_lock()) lk->unlock();

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

    // This search is inclusive and can't return a message outside [A,B]
    std::vector<T*> getRange(uint64_t utimeA, uint64_t utimeB) const
    {
        std::unique_lock<decltype(bufLock)> lk(bufLock);

        // See reason for linear search given in the get() function
        std::vector<T*> ret;
        for (const MsgType* m : buf) {
            uint64_t mUtime = getMsgUtime(m);
            if (utimeA <= mUtime && mUtime <= utimeB)
                ret.push_back(new T(*m));
        }
        return ret;
    }

    size_t expireBefore(uint64_t utime)
    {
        size_t ret = 0;
        std::unique_lock<decltype(bufLock)> lk(bufLock);
        // RRR: earlier we *don't* make the assumption that the buffer is ordered by utime,
        //      but here you are making that assumption. We should make it consistent
        // Expire things that are too old
        while (!buf.empty()) {
            if (getMsgUtime(buf.front()) >= utime) break;
            MsgType* tmp = buf.front();
            delete tmp;
            buf.pop_front();
            ++ret;
        }
        return ret;
    }

    // hostUtime is only used and required when _msg does not have an
    // internal utime field
    // Returns utime of message
    virtual uint64_t newMsg(const T* _msg, uint64_t hostUtime = UINT64_MAX)
    {
        MsgType* tmp = new MsgType(*_msg, hostUtime);
        uint64_t tmpUtime = getMsgUtime(tmp);

        {
            std::unique_lock<decltype(bufLock)> lk(bufLock);

            // Expire due to buffer being full
            if (buf.size() == bufMax) {
                MsgType* tmp = buf.front();
                delete tmp;
                buf.pop_front();
            }

            // RRR: I actually think it is inconsistent to expire messages that are
            //      before (this_utime - maxTimeErr) because you can have trackers that
            //      have *low* tolerance for time error but may need to look up messages
            //      a fair distance in the past. This might be a higher level issue with
            //      the use of maxTimeErr throughout trackers, but I think we should
            //      discuss it.
            // Expire things that are too old
            expireBefore(tmpUtime > maxTimeErr_us ? tmpUtime - maxTimeErr_us : 0);

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

        if (thr) {
            if (callbackLock.try_lock()) {
                if (callbackMsg) delete callbackMsg;
                callbackMsg = new MsgType(*_msg, hostUtime);
                callbackLock.unlock();
                callbackCv.notify_all();
            }
        }

        return tmpUtime;
    }

    double getHz() const
    {
        std::unique_lock<decltype(bufLock)> lk(bufLock);
        return 1e6 / hzFilter[Filter::LOW_PASS];
    }

    uint64_t lastMsgHostUtime() const
    {
        {
            std::unique_lock<decltype(bufLock)> lk(bufLock);
            if (!buf.empty()) return lastHostUtime;
        }
        return UINT64_MAX;
    }

    double getJitterUs() const
    {
        std::unique_lock<decltype(bufLock)> lk(bufLock);
        return sqrt(jitterFilter[Filter::LOW_PASS]);
    }



  private:
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
};

// Note: Remember you have to call the Tracker<T> constructor directly
//       yourself due to the virtual inheritance below!
template <typename T>
class MessageTracker : public virtual Tracker<T>
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

// RRR: I feel like ... it's time to break this up into more than 1 file

// This class will attempt to synchronize messages of type Type1Tracker::ZcmType to
// messages of type Type2Tracker::ZcmType.
//
// If you're interested in the details, here they are:
//
// Whenever a Synchronizer receives a message of Type1Tracker::ZcmType
// it checks to see if there is a message of type Type2Tracker::ZcmType that has a utime no
// earlier than than the received message of ZcmType1. If there is a message of
// type Type2Tracker::ZcmType that satisfies this criteria, then `get` will return a valid pair
// with the original message and the result of `t2.get(msg1.utime)`. If there is
// not a later message, this class will wait until a message of type Type2Tracker::ZcmType is
// received that is no earlier than the message of Type1Tracker::ZcmType.
template <typename Type1Tracker, typename Type2Tracker>
class SynchronizedMessageTracker
{
  public:
    typedef std::function<void(const typename Type1Tracker::ZcmType*,
                                     typename Type2Tracker::ZcmType*,
                                     void*)> callback;

  private:
    class TrackerOverride1 : public Type1Tracker
    {
      private:
        SynchronizedMessageTracker* smt;

      public:
        uint64_t newMsg(const typename Type1Tracker::ZcmType* _msg,
                        uint64_t hostUtime = UINT64_MAX) override
        {
            uint64_t utime = Type1Tracker::newMsg(_msg, hostUtime);
            smt->process1(_msg, utime);
            return utime;
        }

        // RRR: curious because I don't know, how does this behave when
        //      Type1Tracker == Tracker<Type1Tracker::ZcmType> ?
        //      Is it ok that you are calling the same constructor twice?
        TrackerOverride1(zcm::ZCM* zcmLocal, std::string channel,
                         double maxTimeErr, size_t maxMsgs,
                         SynchronizedMessageTracker* smt) :
            Tracker<typename Type1Tracker::ZcmType>(maxTimeErr, maxMsgs),
            Type1Tracker(zcmLocal, channel, maxTimeErr, maxMsgs), smt(smt)
        {}
    };

    class TrackerOverride2 : public Type2Tracker
    {
      private:
        SynchronizedMessageTracker* smt;

      public:
        uint64_t newMsg(const typename Type2Tracker::ZcmType* _msg,
                        uint64_t hostUtime = UINT64_MAX) override
        {
            uint64_t utime = Type2Tracker::newMsg(_msg, hostUtime);
            smt->process2(utime);
            return utime;
        }

        TrackerOverride2(zcm::ZCM* zcmLocal, std::string channel,
                         double maxTimeErr, size_t maxMsgs,
                         SynchronizedMessageTracker* smt) :
            Tracker<typename Type2Tracker::ZcmType>(maxTimeErr, maxMsgs),
            Type2Tracker(zcmLocal, channel, maxTimeErr, maxMsgs), smt(smt) {}
    };

    // RRR: this class requires that messages be coming in utime order, which wasn't an
    //      assumption the original trackers make, so you probably need to make that clear
    void process1(const typename Type1Tracker::ZcmType* msg, uint64_t utime)
    {
        auto it = t2.crbegin();
        if (t2.getMsgUtime(*it) >= utime) {
            auto msg2 = t2.get(utime);
            if (msg2) onSynchronizedMsg(msg, msg2, usr);
        }
    }

    void process2(uint64_t utime)
    {
        // RRR: won't this trigger multiple times per t1 message now?
        for (auto it = t1.cbegin(); it < t1.cend(); ++it) {
            if (t1.getMsgUtime(*it) < utime) {
                auto msg2 = t2.get(t1.getMsgUtime(*it));
                if (msg2) onSynchronizedMsg(*it, msg2, usr);
            }
        }
    }

    TrackerOverride1 t1;
    TrackerOverride2 t2;

    callback onSynchronizedMsg;
    void* usr;

  public:
    SynchronizedMessageTracker(zcm::ZCM* zcmLocal, size_t maxMsgPairs,
                               std::string channel_1, double maxTimeErr_1, size_t maxMsgs_1,
                               std::string channel_2, double maxTimeErr_2, size_t maxMsgs_2,
                               callback onSynchronizedMsg, void* usr = nullptr) :
        t1(zcmLocal, channel_1, maxTimeErr_1, maxMsgs_1, this),
        t2(zcmLocal, channel_2, maxTimeErr_2, maxMsgs_2, this),
        onSynchronizedMsg(onSynchronizedMsg), usr(usr)
    {
        // RRR: so this only works if the internal trackers are actually MessageTrackers,
        //      not any other trackers. It'd be pretty cool if you could make it either
        // RRR: I think this function would throw an error due to the constructor calls
        //      before hitting these static asserts. If you want the reason for the
        //      error to be clear, you should move these static asserts just into
        //      the definition of the class instead of inside this function
        static_assert(std::is_base_of<MessageTracker<typename Type1Tracker::ZcmType>,
                                      Type1Tracker>::value,
                      "Tracker type1 must be an extension of MessageTracker<type1>");
        static_assert(std::is_base_of<MessageTracker<typename Type2Tracker::ZcmType>,
                                      Type2Tracker>::value,
                      "Tracker type2 must be an extension of MessageTracker<type2>");
    }

    friend class ::MessageTrackerTest;
};

}

#undef ZCM_DEBUG
