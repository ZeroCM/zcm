#pragma once
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lockfree Bcast: multi-publisher broadcast to multi-consumer */

typedef struct lf_bcast      lf_bcast_t;
typedef struct lf_bcast_sub  lf_bcast_sub_t;
struct __attribute__((aligned(16))) lf_bcast_sub { char _opaque[64]; };

/* New/delete API */
lf_bcast_t * lf_bcast_new(size_t depth, size_t max_msg_sz);
void         lf_bcast_delete(lf_bcast_t *bcast);

/* Publishing */
void         lf_bcast_pub(lf_bcast_t *b, void *buf);

/* Subscribing */
void         lf_bcast_sub_init(lf_bcast_sub_t *sub, lf_bcast_t *b);
size_t       lf_bcast_sub_drops(lf_bcast_t *sub);
const void * lf_bcast_sub_consume_begin(lf_bcast_sub_t *sub, int64_t timeout);
bool         lf_bcast_sub_consume_end(lf_bcast_sub_t *sub);

/* Buffer pooling API */
void *       lf_bcast_buf_acquire(lf_bcast_t *b);
void         lf_bcast_buf_release(lf_bcast_t *bcast, void *ptr);

/* Advanced API */
void         lf_bcast_footprint(size_t depth, size_t max_msg_sz, size_t *size, size_t *align);
lf_bcast_t * lf_bcast_mem_init(void *mem, size_t depth, size_t max_msg_sz);
lf_bcast_t * lf_bcast_mem_join(void *mem, size_t depth, size_t max_msg_sz);
void         lf_bcast_mem_leave(lf_bcast_t *lf_bcast);

#ifdef __cplusplus
}
#endif
