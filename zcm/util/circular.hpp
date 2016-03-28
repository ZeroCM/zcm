#pragma once
#include <iostream>
#include <cstdlib>
#include <cassert>

// A C++ circular buffer implementation designed for efficiency.
// No unneeded copies or initializations.
// Note: Nothing about this circular buffer is thread-safe
class CircularTest;

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
    Circular() : Circular(1) {}

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
        while (this->_size > 0) removeBack();

        delete[] this->_data;
    }

    size_t size() const
    {
        return this->_size;
    }

    size_t capacity() const
    {
        return this->_capacity;
    }

    bool isEmpty() const
    {
        return this->_size == 0;
    }

    bool isFull() const
    {
        return this->_size == this->_capacity;
    }

    // RRR: a little weird that this doesn't delete the elements
    void clear()
    {
        this->_size = 0;
        this->_front = 0;
        this->_back = 0;
    }

    // RRR: I would note that elt has to be dynamic
    bool pushBack(Element* elt)
    {
        if (isFull()) return false;

        this->_data[this->_back] = elt;

        this->_size++;
        this->_back += 1;

        // Handle wrap around
        if (this->_back == this->_capacity) this->_back = 0;
        return true;
    }

    bool pushFront(Element* elt)
    {
        if (isFull()) return false;

        this->_size++;
        if (this->_front != 0) this->_front -= 1;
        else this->_front = this->_capacity - 1;

        this->_data[this->_front] = elt;

        return true;
    }

    Element* front()
    {
        if (isEmpty()) return nullptr;

        return this->_data[this->_front];
    }

    Element* back()
    {
        if (isEmpty()) return nullptr;

        size_t temp;
        if (this->_back == 0) temp = this->_capacity - 1;
        else temp = this->_back - 1;

        return this->_data[temp];
    }

    void popFront()
    {
        // RRR: this should probably be an isEmpty() -> return. Could return a
        //      bool based on whether the container has any remaining elts
        assert(!isEmpty());

        this->_size--;
        this->_front += 1;

        // Handle wrap around
        if (this->_front == this->_capacity) this->_front = 0;
    }

    void popBack()
    {
        // RRR: this should probably be an isEmpty() -> return. Could return a
        //      bool based on whether the container has any remaining elts
        assert(!isEmpty());

        this->_size--;

        if (this->_back == 0) this->_back = this->_capacity - 1;
        else this->_back--;
    }

    // RRR: I'd change the names of these to "deleteFront()" and "deleteBack()"
    //      another thing that's a little weird is whether the destructor and/or
    //      clear should be deleting the elements (because they never do). So it's
    //      a little weird that sometimes this class handles the dynamic mem, but not
    //      always. It'd almost be better to be able to pass in an element destructor
    //      on construction to better handle the (potentially) virtual memory
    // RRR: on reading the rest of the file, it seems that you intended all the elements
    //      to be dynamic, need to make that clearer
    void removeFront()
    {
        // RRR: this should probably be an isEmpty() -> return. Could return a
        //      bool based on whether the container has any remaining elts
        assert(!isEmpty());

        delete this->_data[this->_front];

        popFront();
    }

    void removeBack()
    {
        // RRR: this should probably be an isEmpty() -> return. Could return a
        //      bool based on whether the container has any remaining elts
        assert(!isEmpty());

        popBack();

        delete this->_data[this->_back];
    }

    void resize(size_t capacity)
    {
        assert(capacity > 0);

        while (this->_size > capacity) removeFront();

        Element** temp = new Element*[capacity]();
        if (this->_size > 0) {
            if (this->_front < this->_back) {
                memcpy(temp,
                       this->_data + this->_front,
                       this->_size * sizeof(Element*));
                this->_front = 0;
                this->_back = this->_size;
            } else if (this->_front >= this->_back) {
                size_t nWrapAround = this->_capacity - this->_front;
                size_t nWrapAroundBytes = nWrapAround * sizeof(Element*);
                memcpy(temp,
                       this->_data + this->_front,
                       nWrapAroundBytes);
                memcpy(temp + nWrapAround,
                       this->_data,
                       this->_back * sizeof(Element*));
                this->_front = 0;
                this->_back = this->_size;
            }
        } else {
            this->_front = 0;
            this->_back = 0;
        }
        this->_capacity = capacity;
        delete[] this->_data;
        this->_data = temp;
    }

    Element* operator[](size_t i)
    {
        if (i >= size())
            return nullptr;

        size_t idx = i + this->_front;
        if (idx >= this->_capacity) idx -= this->_capacity;

        return this->_data[idx];
    }

    friend class CircularTest;
};
