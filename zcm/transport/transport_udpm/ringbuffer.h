#ifndef __zcm_ringbuffer_h__
#define __zcm_ringbuffer_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct _zcm_ringbuf zcm_ringbuf_t;

zcm_ringbuf_t * zcm_ringbuf_new (unsigned int ring_size);
void zcm_ringbuf_free (zcm_ringbuf_t * ring);

/*
 * Allocates a variable-sized chunk of the ring buffer for use by the
 * application.  Returns the pointer to the available chunk.
 */
char * zcm_ringbuf_alloc (zcm_ringbuf_t * ring, unsigned int len);

/*
 * resizes the most recently allocated chunk of the ring buffer.  The newly
 * requested size must be smaller than the original chunk size.
 */
void zcm_ringbuf_shrink_last(zcm_ringbuf_t *ring, const char *buf,
        unsigned int len);

unsigned int zcm_ringbuf_capacity(zcm_ringbuf_t *ring);

unsigned int zcm_ringbuf_used(zcm_ringbuf_t *ring);

/*
 * Releases a previously-allocated chunk of the ring buffer.  Only the most
 * recently allocated, or the least recently allocated chunk can be released.
 */
void zcm_ringbuf_dealloc (zcm_ringbuf_t * ring, char * buf);

#ifdef __cplusplus
}
#endif

#endif
