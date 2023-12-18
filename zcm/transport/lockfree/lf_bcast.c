#include "lf_bcast.h"
#include "lf_pool.h"
#include "lf_util.h"
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ESTIMATED_PUBLISHERS 16
#define CHANNEL_MAXLEN 32

struct __attribute__((aligned(CACHE_LINE_SZ))) lf_bcast
{
  u64      depth_mask;
  u64      max_msg_sz;
  u64      head_idx;
  u64      tail_idx;
  size_t   pool_off;
  char     _pad_2[CACHE_LINE_SZ - 4*sizeof(u64) - sizeof(size_t)];

  lf_ref_t slots[];
};
static_assert(sizeof(lf_bcast_t) == CACHE_LINE_SZ, "");
static_assert(alignof(lf_bcast_t) == CACHE_LINE_SZ, "");

typedef struct sub_impl sub_impl_t;
struct __attribute__((aligned(16))) sub_impl
{
  lf_bcast_t * bcast;
  u64          idx;
  u64          drops;
  lf_ref_t *   active_ref_ptr;
  lf_ref_t     active_ref;
  char         _extra[16];
};
static_assert(sizeof(sub_impl_t) == sizeof(lf_bcast_sub_t), "");
static_assert(alignof(sub_impl_t) == alignof(lf_bcast_sub_t), "");

static inline lf_pool_t *get_pool(lf_bcast_t *b) { return (lf_pool_t*)((char*)b + b->pool_off); }

static uint32_t *wait_addr(lf_bcast_t *b)
{
#if defined(__BYTE_ORDER)
# if __BYTE_ORDER == __LITTLE_ENDIAN
  return (uint32_t*)&b->tail_idx;
# else
  return ((uint32_t*)&b->tail_idx) + 1;
# endif
#else
# error "__BYTE_ORDER is not defined"
#endif
}

static void wait(sub_impl_t *sub, int64_t *_timeout)
{
  // Wait for next message with futex_wait. Unfortunately, linux only supports
  // 32-bit futex, so we truncate to the lower 32-bits. We will assume little-endian
  // here. This is checked in lf_machine_assumptions.h


  // TODO: Explain the subtlties of using lower 32-bits

  int64_t start = wallclock();

  int64_t timeout = *_timeout;
  struct timespec tm = {
    .tv_sec = timeout / (int64_t)1e9,
    .tv_nsec = timeout % (int64_t)1e9,
  };

  uint32_t   val  = (uint64_t)sub->idx;
  uint32_t * addr = (uint32_t*)&sub->bcast->tail_idx;
  syscall(SYS_futex, addr, FUTEX_WAIT, val, tm, NULL, 0);

  int64_t dt = wallclock() - start;
  *_timeout = timeout > dt ? timeout - dt : 0;
}

static void wake(lf_bcast_t *bcast)
{
  uint32_t * addr = (uint32_t*)&bcast->tail_idx;
  syscall(SYS_futex, addr, FUTEX_WAKE, INT32_MAX, 0, NULL, 0);
}

lf_bcast_t * lf_bcast_new(size_t depth, size_t max_msg_sz)
{
  size_t mem_sz, mem_align;
  lf_bcast_footprint(depth, max_msg_sz, &mem_sz, &mem_align);

  void *mem = NULL;
  int ret = posix_memalign(&mem, mem_align, mem_sz);
  if (ret != 0) return NULL;

  lf_bcast_t *bcast = lf_bcast_mem_init(mem, depth, max_msg_sz);
  if (!bcast) {
    free(mem);
    return NULL;
  }

  return bcast;
}

void lf_bcast_delete(lf_bcast_t *bcast)
{
  free(bcast);
}

static void try_drop_head(lf_bcast_t *b, u64 head_idx)
{
  // TODO AUDIT
  lf_ref_t head_cur = b->slots[head_idx & b->depth_mask];
  if (!LF_U64_CAS(&b->head_idx, head_idx, head_idx+1)) return;

  // FIXME: THIS IS WHERE WE MIGHT LOSE SHARED RESOURCES
  u64 buf_off = head_cur.val;
  void *buf = (char*)b + buf_off;
  lf_pool_release(get_pool(b), buf);
}

void lf_bcast_pub(lf_bcast_t *b, void *buf)
{
  u64 buf_off = (char*)buf - (char*)b;
  // FIXME: ASSERT TO CHECK THE BUF IS FROM THE POOL

  while (1) {
    u64        head_idx = b->head_idx;
    u64        tail_idx = b->tail_idx;
    lf_ref_t * tail_ptr = &b->slots[tail_idx & b->depth_mask];
    lf_ref_t   tail_cur = *tail_ptr;
    LF_BARRIER_ACQUIRE();

    // Stale tail pointer? Try to advance it..
    if (tail_cur.tag == tail_idx) {
      LF_U64_CAS(&b->tail_idx, tail_idx, tail_idx+1);
      LF_PAUSE();
      continue;
    }

    // Stale tail_idx? Try again..
    if (tail_cur.tag >= tail_idx) {
      LF_PAUSE();
      continue;
    }

    // Slot currently used.. full.. roll off the head
    if (head_idx <= tail_cur.tag) {
      assert(head_idx == tail_cur.tag);
      try_drop_head(b, head_idx);
      LF_PAUSE();
      continue;
    }

    // So far so good, try to append the tail
    lf_ref_t tail_next = LF_REF_MAKE(tail_idx, buf_off);
    if (!LF_REF_CAS(tail_ptr, tail_cur, tail_next)) {
      LF_PAUSE();
      continue;
    }

    // Success, try to update the tail idx. If we fail, it's okay.
    // We've already committed the append. Another pub might race here and that's okay.
    LF_U64_CAS(&b->tail_idx, tail_idx, tail_idx+1);

    // Wake up all consumers
    wake(b);

    // All done
    return;
  }
}

void lf_bcast_sub_init(lf_bcast_sub_t *_sub, lf_bcast_t *b)
{
  sub_impl_t *sub = (sub_impl_t*)_sub;
  memset(sub, 0, sizeof(sub_impl_t));
  sub->bcast = b;
  sub->idx = b->tail_idx;
}

const void *lf_bcast_sub_consume_begin(lf_bcast_sub_t *_sub, int64_t timeout)
{
  sub_impl_t *sub = (sub_impl_t*)_sub;
  lf_bcast_t *b   = sub->bcast;

  while (1) {
    u64 tail_idx = b->tail_idx;
    LF_BARRIER_ACQUIRE();
    if (sub->idx == tail_idx) {
      if (timeout > 0) wait(sub, &timeout);
      continue;
    }

    lf_ref_t * ref_ptr = &b->slots[sub->idx & b->depth_mask];
    lf_ref_t   ref     = *ref_ptr;

    LF_BARRIER_ACQUIRE();

    if (ref.tag != sub->idx) { // We've fallen behind and the message we wanted was dropped?
      sub->idx++;
      sub->drops++;
      LF_PAUSE();
      continue;
    }

    u64 buf_off = ref.val;
    void *buf = (char*)b + buf_off;

    // save the ref and ref_ptr so we can check validity again later
    sub->active_ref_ptr = ref_ptr;
    sub->active_ref = ref;

    return buf;
  }
}

bool lf_bcast_sub_consume_end(lf_bcast_sub_t *_sub)
{
   sub_impl_t *sub   = (sub_impl_t*)_sub;

   LF_BARRIER_ACQUIRE();

   lf_ref_t ref = *sub->active_ref_ptr;
   bool valid = LF_REF_EQUAL(ref, sub->active_ref); // Did data change while reading?
   if (!valid) {
     sub->drops++;
   }

   sub->idx++;
   return valid;
}

void * lf_bcast_buf_acquire(lf_bcast_t *b)
{
  return lf_pool_acquire(get_pool(b));
}

void lf_bcast_buf_release(lf_bcast_t *b, void *ptr)
{
  lf_pool_release(get_pool(b), ptr);
}

void lf_bcast_footprint(size_t depth, size_t max_msg_sz, size_t *_size, size_t *_align)
{
  size_t elt_sz    = LF_ALIGN_UP(max_msg_sz, alignof(u128));
  size_t pool_elts = depth + ESTIMATED_PUBLISHERS;

  size_t pool_size, pool_align;
  lf_pool_footprint(pool_elts, elt_sz, &pool_size, &pool_align);

  size_t size = sizeof(lf_bcast_t);
  size += depth * sizeof(lf_ref_t);
  size = LF_ALIGN_UP(size, pool_align);
  size += pool_size;

  if (_size)  *_size  = size;
  if (_align) *_align = alignof(lf_bcast_t);
}

lf_bcast_t * lf_bcast_mem_init(void *mem, size_t depth, size_t max_msg_sz)
{
  if (!LF_IS_POW2(depth)) return NULL;
  if (max_msg_sz == 0) return NULL;

  size_t elt_sz    = LF_ALIGN_UP(max_msg_sz, alignof(u128));
  size_t pool_elts = depth + ESTIMATED_PUBLISHERS;

  size_t pool_size, pool_align;
  lf_pool_footprint(pool_elts, elt_sz, &pool_size, &pool_align);

  size_t size     = sizeof(lf_bcast_t) + depth * sizeof(lf_ref_t);
  size_t pool_off = LF_ALIGN_UP(size, pool_align);
  void * pool_mem = (char*)mem + pool_off;

  lf_bcast_t *b = (lf_bcast_t*)mem;
  b->depth_mask = depth-1;
  b->max_msg_sz = max_msg_sz;
  b->head_idx = 1; /* Start from 1 because we use 0 to mean "unused" */
  b->tail_idx = 1; /* Start from 1 because we use 0 to mean "unused" */
  b->pool_off = pool_off;

  memset(b->slots, 0, depth * sizeof(lf_ref_t));

  lf_pool_t *pool = lf_pool_mem_init(pool_mem, pool_elts, elt_sz);
  if (!pool) return NULL;
  assert(pool == pool_mem);

  return b;
}

lf_bcast_t * lf_bcast_mem_join(void *mem, size_t depth, size_t max_msg_sz)
{
  lf_bcast_t *bcast = (lf_bcast_t *)mem;
  if (depth-1 != bcast->depth_mask) return NULL;
  if (max_msg_sz != bcast->max_msg_sz) return NULL;

  size_t elt_sz    = LF_ALIGN_UP(max_msg_sz, alignof(u128));
  size_t pool_elts = depth + ESTIMATED_PUBLISHERS;

  void * pool_mem = (char*)mem + bcast->pool_off;
  lf_pool_t * pool = lf_pool_mem_join(pool_mem, pool_elts, elt_sz);
  if (!pool) return NULL;
  assert(pool == pool_mem);

  return bcast;
}

void lf_bcast_mem_leave(lf_bcast_t *b)
{
  lf_pool_t *pool = get_pool(b);
  lf_pool_mem_leave(pool);
}
