#pragma once

#include "zcm/util/queue.hpp"

#include <mutex>
#include <condition_variable>
#include <atomic>

// A thread-safe C++ queue implementation designed for efficiency.
// No unneeded copies or initializations.
// Note: we wrap the Queue implementation
template<class Element>
class ThreadsafeQueue
{
    Queue<Element> queue;

    std::mutex mut;
    std::condition_variable cond;
    volatile int wakeupNum = 0;

  public:
    ThreadsafeQueue(size_t size) : queue(size) {}
    ~ThreadsafeQueue() {}

    bool hasFreeSpace()
    {
        std::unique_lock<std::mutex> lk(mut);
        return queue.hasFreeSpace();
    }

    bool hasMessage()
    {
        std::unique_lock<std::mutex> lk(mut);
        return queue.hasMessage();
    }

    // Wait for hasFreeSpace() and then push the new element
    // Returns true if the value was pushed, otherwise it
    // was forcibly awoken by forceWakeups()
    template<class... Args>
    bool push(Args&&... args)
    {
        std::unique_lock<std::mutex> lk(mut);
        int localWakeupNum = wakeupNum;
        cond.wait(lk, [&](){
            return localWakeupNum < wakeupNum ||
                   queue.hasFreeSpace();
        });
        if (localWakeupNum < wakeupNum)
            return false;
        queue.push(std::forward<Args>(args)...);
        cond.notify_one();
        return true;
    }

    // Wait for hasMessage() and then return the top element
    // Always returns a valid Element* except when is was
    // forcibly awoken by forceWakeups(). In such a case
    // nullptr is returned to the user
    Element *top()
    {
        std::unique_lock<std::mutex> lk(mut);
        int localWakeupNum = wakeupNum;
        cond.wait(lk, [&](){
            return localWakeupNum < wakeupNum ||
                   queue.hasMessage();
        });
        if (localWakeupNum < wakeupNum) {
            return nullptr;
        } else {
            Element& elt = queue.top();
            return &elt;
        }
    }

    // Requires that hasMessage() == true
    void pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        queue.pop();
        cond.notify_one();
    }

    // Force all blocked threads to wakeup and return from
    // whichever methods are blocking them
    void forceWakeups()
    {
        std::unique_lock<std::mutex> lk(mut);
        wakeupNum++;
        cond.notify_all();
    }

    void waitForEmpty()
    {
        std::unique_lock<std::mutex> lk(mut);
        int localWakeupNum = wakeupNum;
        cond.wait(lk, [&](){
            return localWakeupNum < wakeupNum ||
                   !queue.hasMessage();
        });
    }
};
