#ifndef _ZCM_TRANS_UDPM_RINGBUFFER_HPP
#define _ZCM_TRANS_UDPM_RINGBUFFER_HPP

#include "udpm.hpp"

struct RingbufferRec;
struct Ringbuffer
{
    Ringbuffer(u32 ring_size);
    ~Ringbuffer();

    /*
     * Allocates a variable-sized chunk of the ring buffer for use by the
     * application.  Returns the pointer to the available chunk.
     */
    char *alloc(u32 len);

    /*
     * Releases a previously-allocated chunk of the ring buffer.  Only the most
     * recently allocated, or the least recently allocated chunk can be released.
     */
    void dealloc(char *buf);

    /*
     * resizes the most recently allocated chunk of the ring buffer.  The newly
     * requested size must be smaller than the original chunk size.
     */
    void shrink_last(const char *buf, u32 len);

    u32 get_capacity() { return size; }
    u32 get_used() { return used; }

    /*************************************************************
     * Members: never access these directly
     ************************************************************/
    char *data;
    unsigned int   size;                 // allocated size of data
    unsigned int   used = 0;            // total bytes currently allocated

    RingbufferRec *head = nullptr;
    RingbufferRec *tail = nullptr;
};

#endif
