#pragma once
#include <iostream>
#include <cstdlib>
#include <cassert>

// A C++ circular buffer implementation designed for efficiency.
// No unneeded copies or initializations.
// Note: Nothing about this circular buffer is thread-safe
template <typename Element>
class Circular
{
  private:
    size_t   _capacity;
    size_t   _front;
    size_t   _back;
    size_t   _size;
    Element** _data;

  public:
    Circular(size_t capacity)
    {
        assert(capacity > 0);

        this->_capacity = capacity;
        this->_data = new Element*[this->_capacity];
        assert(this->_data);

        clear();
    }

    ~Circular()
    {
        delete[] this->_data;
    }

    size_t size() const
    {
        return this->_size;
    }

    bool isEmpty() const
    {
        return this->_size == 0;
    }

    bool isFull() const
    {
        return this->_size == this->_capacity;
    }

    void clear()
    {
        this->_size = 0;
        this->_front = 0;
        this->_back = 0;
    }

    bool pushBack(Element* elt)
    {
        if (isFull())
            return false;

        this->_data[this->_back] = elt;

        this->_size++;
        this->_back += 1;

        // Handle wrap around
        if (this->_back == this->_capacity) this->_back = 0;
        return true;
    }

    bool pushFront(Element* elt)
    {
        if (isFull())
            return false;

        this->_size++;
        if (this->_front != 0) this->_front -= 1;
        else this->_front = this->_capacity - 1;

        this->_data[this->_front] = elt;

        return true;
    }

    Element* front()
    {
        if (isEmpty())
            return nullptr;

        return this->_data[this->_front];
    }

    Element* back()
    {
        if (isEmpty())
            return nullptr;

        size_t temp;
        if (this->_back == 0) temp = this->_capacity - 1;
        else temp = this->_back - 1;

        return this->_data[temp];
    }

    void popFront()
    {
        assert(!isEmpty());

        this->_size--;
        this->_front += 1;

        // Handle wrap around
        if (this->_front == this->_capacity) this->_front = 0;
    }

    void popBack()
    {
        assert(!isEmpty());

        this->_size--;

        if (this->_back == 0) this->_back = this->_capacity - 1;
        else this->_back--;
    }

    void removeFront()
    {
        assert(!isEmpty());

        delete this->_data[this->_front];

        popFront();
    }

    void removeBack()
    {
        assert(!isEmpty());

        popBack();

        delete this->_data[this->_back];
    }

    Element* operator[](size_t i)
    {
        if (i >= size())
            return nullptr;

        int idx = i + this->_front;
        if (idx > this->_capacity)
            idx -= this->_capacity;

        return this->_data[idx];
    }
};
