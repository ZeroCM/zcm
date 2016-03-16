#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <zcm/zcm-cpp.hpp>

#include "util/Circular.hpp"

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
            buf->pushBack(tmp);
        }
        newMsg.notify_all();
    }

    MessageTracker() {}

  public:
    MessageTracker(zcm::ZCM* zcmLocal, std::string channel,
                   callback onMsg = nullptr, void* usr = nullptr)
        : zcmLocal(zcmLocal), onMsg(onMsg), usr(usr)
    {
        buf = new Circular<T>(20);
        if (onMsg != nullptr)
            thr = new std::thread(&MessageTracker<T>::callbackThreadFunc, this);
        s = zcmLocal->subscribe(channel, &MessageTracker<T>::handle, this);
    }

    ~MessageTracker()
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
    T* getNonBlocking()
    {
        T* ret = nullptr;
        {
            std::unique_lock<std::mutex> lk(bufLock);
            if (!buf->isEmpty())
                ret = new T(*buf->back());
        }
        return ret;
    }

    // You must free the memory returned here
    T* get()
    {
        T* ret;

        {
            std::unique_lock<std::mutex> lk(bufLock);
            if (buf->isEmpty())
                newMsg.wait(lk, [&]{ return !buf->isEmpty() || done; });
            if (done) return nullptr;
            ret = new T(*buf->back());
        }

        return ret;
    }
};
