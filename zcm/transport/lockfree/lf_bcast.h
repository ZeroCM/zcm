#pragma once
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lockfree Bcast: multi-publisher broadcast to multi-consumer */

typedef struct lf_bcast     lf_bcast_t;
typedef struct lf_bcast_sub lf_bcast_sub_t;
struct __attribute__((aligned(16))) lf_bcast_sub { char _opaque[32]; };

/* Basic API */
lf_bcast_t * lf_bcast_new(size_t depth, size_t max_msg_sz);
void         lf_bcast_delete(lf_bcast_t *bcast);
bool         lf_bcast_pub(lf_bcast_t *b, const char *channel, size_t channel_len, void *buf, size_t len);
void         lf_bcast_sub_begin(lf_bcast_sub_t *sub, lf_bcast_t *b);
bool         lf_bcast_sub_next(lf_bcast_sub_t *sub, char *channel_buf, void * buf, int64_t timeout,
                               size_t * _out_len, size_t *_out_drops);

/* Advanced API */
void         lf_bcast_footprint(size_t depth, size_t max_msg_sz, size_t *size, size_t *align);
lf_bcast_t * lf_bcast_mem_init(void *mem, size_t depth, size_t max_msg_sz);
lf_bcast_t * lf_bcast_mem_join(void *mem, size_t depth, size_t max_msg_sz);
void         lf_bcast_mem_leave(lf_bcast_t *lf_bcast);

#ifdef __cplusplus
}
#endif
