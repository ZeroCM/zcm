#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Lockfree Queue: MPMC */

typedef struct lf_queue lf_queue_t;

/* Basic API */
lf_queue_t * lf_queue_new(size_t depth);
void         lf_queue_delete(lf_queue_t *q);
bool         lf_queue_enqueue(lf_queue_t *q, uint64_t val);
bool         lf_queue_dequeue(lf_queue_t *q, uint64_t *_val);

/* Advanced API */
void         lf_queue_footprint(size_t depth, size_t *size, size_t *align);
lf_queue_t * lf_queue_mem_init(void *mem, size_t depth);
lf_queue_t * lf_queue_mem_join(void *mem, size_t depth);
void         lf_queue_mem_leave(lf_queue_t *lf_queue);
