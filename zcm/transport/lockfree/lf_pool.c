#include "lf_pool.h"
#include "lf_util.h"

struct __attribute__((aligned(CACHE_LINE_SZ))) lf_pool
{
  size_t   num_elts;
  size_t   elt_sz;
  u64      tag_next;
  u64      _pad1;
  lf_ref_t head;
  char     _pad2[CACHE_LINE_SZ - 2*sizeof(size_t) - 2*sizeof(u64) - sizeof(lf_ref_t)];

  char   mem[];
};
static_assert(sizeof(lf_pool_t) == CACHE_LINE_SZ, "");
static_assert(alignof(lf_pool_t) == CACHE_LINE_SZ, "");

lf_pool_t * lf_pool_new(size_t num_elts, size_t elt_sz)
{
  size_t mem_sz, mem_align;
  lf_pool_footprint(num_elts, elt_sz, &mem_sz, &mem_align);

  void *mem = NULL;
  int ret = posix_memalign(&mem, mem_align, mem_sz);
  if (ret != 0) return NULL;

  lf_pool_t *pool = lf_pool_mem_init(mem, num_elts, elt_sz);
  if (!pool) {
    free(mem);
    return NULL;
  }

  return pool;
}

void lf_pool_delete(lf_pool_t *pool)
{
  free(pool);
}

void *lf_pool_acquire(lf_pool_t *pool)
{
  while (1) {
    lf_ref_t cur = pool->head;
    if (LF_REF_IS_NULL(cur)) return NULL;

    u64        elt_off = cur.val;
    lf_ref_t * elt     = (lf_ref_t*)((char*)pool + elt_off);
    lf_ref_t   next    = *elt;

    if (!LF_REF_CAS(&pool->head, cur, next)) {
      LF_PAUSE();
      continue;
    }
    return elt;
  }
}

void lf_pool_release(lf_pool_t *pool, void *elt)
{
  u64      elt_off = (u64)((char*)elt - (char*)pool);
  u64      tag     = LF_ATOMIC_INC(&pool->tag_next);
  lf_ref_t next    = LF_REF_MAKE(tag, elt_off);

  while (1) {
    lf_ref_t cur = pool->head;
    *(lf_ref_t*)elt = cur;

    if (!LF_REF_CAS(&pool->head, cur, next)) {
      LF_PAUSE();
      continue;
    }
    return;
  }
}

void lf_pool_footprint(size_t num_elts, size_t elt_sz, size_t *_size, size_t *_align)
{
  elt_sz = LF_ALIGN_UP(elt_sz, alignof(u128));

  if (_size)  *_size  = sizeof(lf_pool_t) + elt_sz * num_elts;
  if (_align) *_align = alignof(lf_pool_t);
}

lf_pool_t * lf_pool_mem_init(void *mem, size_t num_elts, size_t elt_sz)
{
  if (elt_sz == 0) return NULL;
  elt_sz = LF_ALIGN_UP(elt_sz, alignof(u128));

  lf_pool_t *lf_pool = (lf_pool_t *)mem;
  lf_pool->num_elts = num_elts;
  lf_pool->elt_sz   = elt_sz;
  lf_pool->tag_next = 0;
  lf_pool->head     = LF_REF_NULL;

  char *ptr = lf_pool->mem + num_elts * elt_sz;
  for (size_t i = num_elts; i > 0; i--) {
    ptr -= elt_sz;
    lf_pool_release(lf_pool, ptr);
  }

  return lf_pool;
}

lf_pool_t * lf_pool_mem_join(void *mem, size_t num_elts, size_t elt_sz)
{
  lf_pool_t *pool = (lf_pool_t *)mem;
  if (num_elts != pool->num_elts) return NULL;
  if (elt_sz != pool->elt_sz) return NULL;
  return pool;
}

void lf_pool_mem_leave(lf_pool_t *lf_pool)
{
  /* no-op at the moment */
}
