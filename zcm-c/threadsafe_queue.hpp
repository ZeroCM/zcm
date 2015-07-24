#pragma once

#include "queue.hpp"

#include <mutex>
#include <condition_variable>

// A thread-safe C++ queue implementation designed for efficiency.
// No unneeded copies or initializations.
// Note: we wrap the Queue implementation
template<class Element>
class ThreadsafeQueue
{
    Queue<Element> queue;

    std::mutex mut;
    std::condition_variable cond;

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
    template<class... Args>
    void push(Args&&... args)
    {
        std::unique_lock<std::mutex> lk(mut);
        cond.wait(lk, [&](){ return queue.hasFreeSpace(); });
        queue.push(std::forward<Args>(args)...);
        cond.notify_one();
    }

    // Wait for hasMessage() and then return the top element
    Element& top()
    {
        std::unique_lock<std::mutex> lk(mut);
        cond.wait(lk, [&](){ return queue.hasMessage(); });
        return queue.top();
    }

    // Requires that hasMessage() == true
    void pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        queue.pop();
        cond.notify_one();
    }
};
