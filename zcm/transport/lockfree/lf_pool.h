#pragma once
#include <stdbool.h>
#include <stdlib.h>

/* Lockfree Pool */

typedef struct lf_pool lf_pool_t;

/* Basic API */
lf_pool_t * lf_pool_new(size_t num_elts, size_t elt_sz, size_t elt_align);
void        lf_pool_delete(lf_pool_t *lf_pool);
void *      lf_pool_acquire(lf_pool_t *lf_pool);
void        lf_pool_release(lf_pool_t *lf_pool, void *elt);

/* Advanced API */
bool        lf_pool_footprint(size_t num_elts, size_t elt_sz, size_t elt_align, size_t *size, size_t *align);
lf_pool_t * lf_pool_mem_init(void *mem, size_t num_elts, size_t elt_sz, size_t elt_align);
lf_pool_t * lf_pool_mem_join(void *mem, size_t num_elts, size_t elt_sz, size_t elt_align);
void        lf_pool_mem_leave(lf_pool_t *lf_pool);
