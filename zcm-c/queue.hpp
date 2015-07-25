#pragma once

#include <utility>
#include <cstring>
#include <cassert>

// A C++ queue implementation designed for efficiency.
// No unneeded copies or initializations.
// Note: Nothing about this queue is thread-safe
template<class Element>
class Queue
{
    Element *queue;
    size_t   size;
    size_t   front = 0;
    size_t   back = 0;

    size_t incIdx(size_t i)
    {
        // Note: one might be tempted to write '(i+1)%size' here
        // But, the modulus operation is slower than possibly missing
        // a branch every once in a while. The branch is almost always
        // Not Taken

        size_t nextIdx = i+1;
        if (nextIdx == size)
            return 0;
        return nextIdx;
    }

  public:
    Queue(size_t size) : size(size)
    {
        // We intentionally use malloc here to avoid intiailized
        queue = (Element*) malloc(size * sizeof(Element));
    }

    ~Queue()
    {
        // We need to deconstruct any elements still in the queue
        while (hasMessage()) pop();
        free(queue);
    }

    bool hasFreeSpace()
    {
        return front != incIdx(back);
    }

    bool hasMessage()
    {
        return front != back;
    }

    // Requires that hasFreeSpace() == true
    template<class... Args>
    void push(Args&&... args)
    {
        assert(hasFreeSpace());

        // Initialize the Element by forwarding the parameter pack
        // directly to the constructor called via Placement New
        new (&queue[back]) Element(std::forward<Args>(args)...);
        back = incIdx(back);
    }

    // Requires that hasMessage() == true
    Element& top()
    {
        assert(hasMessage());
        return queue[front];
    }

    // Requires that hasMessage() == true
    void pop()
    {
        assert(hasMessage());
        // Manually call the destructor
        queue[front].~Element();
        front = incIdx(front);
    }

  private:
    Queue(const Queue& other) = delete;
    Queue(Queue&& other) = delete;
    Queue& operator=(const Queue& other) = delete;
    Queue& operator=(Queue&& other) = delete;
};
