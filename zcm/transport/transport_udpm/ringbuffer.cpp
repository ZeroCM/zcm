#include "ringbuffer.hpp"

#define ALIGNMENT 32    /* must be power of 2 */
#define MAGIC 0x067f8687
#define ENABLE_SELFTEST false

struct RingbufferRec
{
    i32            magic;
    RingbufferRec *prev;
    RingbufferRec *next;
    u32            length;
    char           buf[];
};

static void selftest(Ringbuffer *ring)
{
    if (!ENABLE_SELFTEST)
        return;

    RingbufferRec *prev = NULL;
    RingbufferRec *rec = ring->head;

    if (!rec) {
        assert(!ring->tail);
        assert(ring->used == 0);
        return;
    }

    int total_length = 0;

    while (1) {
        assert(rec->prev == prev);
        assert(rec->magic == MAGIC);

        total_length += rec->length;

        if (!rec->next)
            break;

        prev = rec;
        rec = rec->next;
    }

    assert(ring->tail == rec);
    assert(total_length == (int)ring->used);

    // check for loops?
}

Ringbuffer::Ringbuffer(u32 ring_size)
{
    data = new char[ring_size];
    size = ring_size;
}

Ringbuffer::~Ringbuffer()
{
    delete[] data;
}

char *Ringbuffer::alloc(unsigned int len)
{
    // Two possible configurations of the ring buffer:
    //
    // [         XXXXXXXXXXXX                  ]
    //           ^head      ^tail
    //
    // [XXXXX                             XXXXX]
    //      ^tail                         ^head
    // XXX add me back!
    //ringbuf_self_test(ring);

    RingbufferRec *rec = NULL;

    len += sizeof(RingbufferRec);
    len = (len + ALIGNMENT - 1) & (~(ALIGNMENT - 1));

    // Note: The first entry is a special case
    if (!this->head) {
        assert(!this->tail);
        if (len > this->size) {
            return NULL;
        }

        rec = (RingbufferRec*) this->data;
        this->tail = this->head = rec;
        rec->prev = rec->next = NULL;
        rec->length = len;
        this->used += len;
        rec->magic = MAGIC;
    }

    // The general (non-zero) case
    else {
        assert (this->head && this->tail);

        // Try to allocate from the current alloc_pos first; if that
        // fails, try to allocate from offset 0.
        char *candidate1 = ((char*) this->tail) + this->tail->length;

        if (this->head > this->tail) {
            if (candidate1 + len <= (char*)this->head) {
                rec = (RingbufferRec*) candidate1;
            } else {
                return NULL; // no space!
            }
        } else {
            if (candidate1 + len <= this->data + this->size) {
                rec = (RingbufferRec*) candidate1;
            } else if (this->data + len < (char*)this->head) {
                rec = (RingbufferRec*)this->data;
            } else {
                return NULL; // no space!
            }
        }

        rec->length = len;
        this->used  += len;

        // update links
        rec->prev = this->tail;
        rec->next = NULL;
        if (rec->prev)
            rec->prev->next = rec;
        this->tail = rec;
        rec->magic = MAGIC;
    }

    assert(rec != NULL);
    selftest(this);
    return rec->buf;
}

void Ringbuffer::shrink_last(const char *buf, u32 newlen)
{
    selftest(this);
    RingbufferRec *rec = (RingbufferRec*)(buf - offsetof(RingbufferRec, buf));

    // make sure this is the most recent alloc
    assert(rec == this->tail);
    assert (rec->magic == MAGIC);

    // compute the new size
    newlen += sizeof(RingbufferRec);
    newlen = (newlen + ALIGNMENT - 1) & (~(ALIGNMENT - 1));

    assert (rec->length >= newlen);
    unsigned int shrink_amount = rec->length - newlen;
    rec->length      = newlen;
    this->used      -= shrink_amount;

    assert(this->used >= 0);
    selftest(this);
}

/*
 * Releases a previously-allocated chunk of the ring buffer.  Only the most
 * recently allocated, or the least recently allocated chunk can be released.
 */
void Ringbuffer::dealloc(char *buf)
{
    selftest(this);
    RingbufferRec *rec = (RingbufferRec*)(buf - offsetof(RingbufferRec, buf));

    assert(rec == this->head || rec == this->tail);
    assert(rec->magic == MAGIC);

    this->used -= rec->length;

    if (rec == this->head) {
        this->head = rec->next;
        if (!this->head)
            this->tail = NULL;
        else
            this->head->prev = NULL;
    } else if (rec == this->tail) {
        this->tail = rec->prev;
        if (!this->tail)
            this->head = NULL;
        else
            this->tail->next = NULL;
    }

    assert ((!this->head && !this->tail) ||
           (this->head->prev == NULL && this->tail->next == NULL));
    if (0 == this->used) { assert (!this->head && !this->tail); }

    rec->magic = 0;
    selftest(this);
}
