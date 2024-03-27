#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************************/
/* Lockfree Bcast: multi-publisher broadcast to multi-consumer */

typedef struct lf_bcast      lf_bcast_t;
typedef struct lf_bcast_sub  lf_bcast_sub_t;
struct __attribute__((aligned(16))) lf_bcast_sub { char _opaque[32]; };

/*************************************************************************************************/
/* New/delete API */
lf_bcast_t * lf_bcast_new(size_t depth, size_t elt_sz, size_t elt_align);
void         lf_bcast_delete(lf_bcast_t *bcast);

/*************************************************************************************************/
/* Buffer pooling API */

/* Acquire a buffer element from the internal shm object pool */
void * lf_bcast_buf_acquire(lf_bcast_t *b);

/* Release a buffer element back to the internal shm object pool. This should only be called if
   the buffer will not be published. */
void   lf_bcast_buf_release(lf_bcast_t *bcast, void *ptr);

/*************************************************************************************************/
/* Publishing */

/* Publish a buffer to the tail of the bcast queue. The buffer must be an element obtained
   by calling lf_bcast_buf_acquire. This call cannot fail. When the queue is full, the head
   is removed automatically to make space. By calling this function, the user transfers ownership
   of the buffer ('buf') to the internal queue. They should NOT call lf_bcast_buf_release(). The
   internal queue will call release when the buffer is no longer required. */
void lf_bcast_pub(lf_bcast_t *b, void *buf);

/*************************************************************************************************/
/* Subscribing */

/* Initialize a subscriber endpoint. The subscriber struct is not part of the shared-memory queue.
   Instead, it is a simple struct allocated by the consumer to track their currect position when
   reading the shared bcast queue. */
void  lf_bcast_sub_init(lf_bcast_sub_t *sub, lf_bcast_t *b);

/* Return the number of drops the sub has experienced. If the consumer is too slow, elements it's
   interested will be reclaimed and rewritten. When the consumer tries to read them, it will discover
   they are missing and count them as a drop. */
uint64_t lf_bcast_sub_drops(lf_bcast_sub_t *sub);

/* Begin consuming the next buffer in the queue. If no buffer is available, wait up to 'timeout' nanos.
   If available, return a pointer to the buffer.

   IMPORTANT:
     (1) The returned buffer pointer is NOT owned by this consumer. It is a shared buffer that multiple
         consumer may be reading from in parallel.
     (2) The returned buffer is volatile and may be reclaimed at any time even while the consume copies it.
     (3) Since the buffer is volatile, the consumer should make no assumptions about data integrity and should
         validate any control-flow dependent assumptions (e.g. copying based on a length field in the buffer) */
const void * lf_bcast_sub_consume_begin(lf_bcast_sub_t *sub, int64_t timeout);

/* Finish consuming the buffer and advance the queue. This function returns whether the buffer remained
   valid between lf_bcast_sub_consume_begin() and lf_bcast_sub_consume_end(). Users should consult the
   return value to decide wether to retain the results of the consume or to discard them. It would
   be incorrect to use the data if this function returns 'false'. */
bool lf_bcast_sub_consume_end(lf_bcast_sub_t *sub);

/*************************************************************************************************/
/* Advanced API */

/* Computes the footprint of the memory region (size and alignment) based on the parameters */
bool lf_bcast_footprint(size_t depth, size_t elt_sz, size_t elt_align, size_t *size, size_t *align);

/* Initialize a new memory region into a new bcast queue */
lf_bcast_t * lf_bcast_mem_init(void *mem, size_t depth, size_t elt_sz, size_t elt_align);

/* Join an existing, previously initialized bcast region */
lf_bcast_t * lf_bcast_mem_join(void *mem, size_t depth, size_t elt_sz, size_t elt_align);

/* Leave the bcast region without destorying it */
void lf_bcast_mem_leave(lf_bcast_t *lf_bcast);

#ifdef __cplusplus
}
#endif
