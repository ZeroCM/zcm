#include "lf_bcast.h"
#include "lf_pool.h"
#include "lf_util.h"
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#define LF_BCAST_ALIGN 4096

struct __attribute__((aligned(LF_BCAST_ALIGN))) lf_bcast
{
  u64      depth_mask;   // Precomputed: "depth-1", used as a mask for slot indicies since depth is a power-of-two
  u64      elt_sz;       // Size of the elements used in the queue
  u64      head_idx;     // Index of the head of the queue: slot index is "head_idx % depth"
  u64      tail_idx;     // Index of the tail of the queue: slot index is "tail_idx % depth"
  size_t   pool_off;     // Memory offset to the element pool
  char     _pad[LF_BCAST_ALIGN - 4*sizeof(u64) - sizeof(size_t)];

  lf_ref_t slots[];      // Queue slots: ref is the tuple (tag=queue_idx, val=element_off)
};
static_assert(sizeof(lf_bcast_t) == LF_BCAST_ALIGN, "");
static_assert(alignof(lf_bcast_t) == LF_BCAST_ALIGN, "");
static_assert(offsetof(lf_bcast_t, slots) == LF_BCAST_ALIGN, "");

typedef struct sub_impl sub_impl_t;
struct __attribute__((aligned(16))) sub_impl
{
  lf_bcast_t * bcast;
  u64          idx;
  u64          drops;
  bool         active;
  char         _extra[7];
};
static_assert(sizeof(sub_impl_t) == sizeof(lf_bcast_sub_t), "");
static_assert(alignof(sub_impl_t) == alignof(lf_bcast_sub_t), "");

static inline lf_pool_t *get_pool(lf_bcast_t *b) { return (lf_pool_t*)((char*)b + b->pool_off); }

// Determine the futex wait addresss (depending on endianness)
static inline uint32_t *wait_addr(lf_bcast_t *b)
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

// Futex wrappers
static inline long futex_wait(uint32_t *addr, uint32_t val, struct timespec timeout)
{
  return syscall(SYS_futex, addr, FUTEX_WAIT, val, &timeout, NULL, 0);
}
static inline long futex_wake(uint32_t *addr, uint32_t n_wake)
{
  return syscall(SYS_futex, addr, FUTEX_WAKE, n_wake, NULL, NULL, 0);
}


// Wait for the next message until "*_timeout" nanos. Updates "*_timeout" with the remaining time.
static void wait(sub_impl_t *sub, int64_t *_timeout)
{
  // Wait for next message with futex_wait. Unfortunately, linux only supports
  // 32-bit futex, so we truncate to the lower 32-bits.
  //
  // The state that the futex waits on is the queue tail index. This index is 64-bit so that
  // the index will never wrap around for the lifetime of the program.
  //
  // Justification of 64-bit indicies:
  //   If an element is enqueued once every 100 ns, the index will overrun after
  //   (100 * (1<<64))/1e9/60/60/24/365 = 58494 years
  //
  // However, since futex doesn't support 64-bit we truncate to the lower 32-bits.
  // This means that will will collide on the index every:
  //   (100 * (1<<32))/1e9/60 = 7 sec (assuming sustained 100ns enqueues)
  //
  // The risk of this is significantly smaller than this result might suggest and the
  // impact is minimal.
  //
  // For the collision to occur, one waiter would have to be suspended for >7 sec (realistically
  // likely >1 minute). And then, it has to catch the falling knife on the exact number.
  // Futex wait uses equality comparison in the kernel. All this to say that the risk is
  // quite small.
  //
  // If the collision does ocurr, it means that the receiver misses a wakeup. But, it will
  // get woken up again as soon as the next message is sent.

  int64_t start = wallclock();

  int64_t timeout = *_timeout;
  struct timespec tm = {
    .tv_sec = timeout / (int64_t)1e9,
    .tv_nsec = timeout % (int64_t)1e9,
  };

  uint32_t   val  = (uint32_t)sub->idx;
  uint32_t * addr = wait_addr(sub->bcast);
  futex_wait(addr, val, tm);

  int64_t dt = wallclock() - start;
  *_timeout = dt < timeout ? timeout - dt : 0;
}

// Wake up all waiters
static void wake(lf_bcast_t *bcast)
{
  futex_wake(wait_addr(bcast), INT32_MAX);
}

lf_bcast_t * lf_bcast_new(size_t depth, size_t elt_sz, size_t elt_align)
{
  size_t mem_sz, mem_align;
  if (!lf_bcast_footprint(depth, elt_sz, elt_align, &mem_sz, &mem_align)) {
    return NULL;
  }

  void *mem = NULL;
  int ret = posix_memalign(&mem, mem_align, mem_sz);
  if (ret != 0) return NULL;

  lf_bcast_t *bcast = lf_bcast_mem_init(mem, depth, elt_sz, elt_align);
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

// Dequeue the head element and return a pointer to the element (if successful)
static void *dequeue_head(lf_bcast_t *b, u64 head_idx)
{
  // Advance head. As a side-effect, this thread takes ownership of the old queue buffer
  // and is responsible for releasing it to the pool or re-enqueuing it.
  // If we fail at advacing the head, we just assume some other core must have
  // beat us and we'll return. Caller can retry if they want an element.
  lf_ref_t head_cur = b->slots[head_idx & b->depth_mask];
  if (!LF_U64_CAS(&b->head_idx, head_idx, head_idx+1)) return NULL;

  // Unpack the pool element and return it
  u64 buf_off = head_cur.val;
  void *buf = (char*)b + buf_off;
  return buf;
}

static void try_drop_head(lf_bcast_t *b, u64 head_idx)
{
  void *buf = dequeue_head(b, head_idx);
  if (!buf) return;
  lf_pool_release(get_pool(b), buf);
}

void lf_bcast_pub(lf_bcast_t *b, void *buf)
{
  // Compute the buffer offset with sanity checks
  assert((char*)buf > (char*)b && "invalid buffer");
  u64 buf_off = (char*)buf - (char*)b;

  while (1) {
    // Start of the trial: load all the shared state to the local stack
    u64        head_idx = b->head_idx;
    u64        tail_idx = b->tail_idx;
    lf_ref_t * tail_ptr = &b->slots[tail_idx & b->depth_mask];
    lf_ref_t   tail_cur = *tail_ptr;
    LF_BARRIER_ACQUIRE();

    // Stale tail pointer? Try to advance it and try again..
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

    // Slot currently used. Queue is full. Roll off the head and try again..
    if (head_idx <= tail_cur.tag) {
      assert(head_idx == tail_cur.tag);
      try_drop_head(b, head_idx);
      LF_PAUSE();
      continue;
    }

    // All the queue consistency checks and corrections passed. Try to append the tail.
    // So far so good, try to append the tail
    lf_ref_t tail_next = LF_REF_MAKE(tail_idx, buf_off);
    if (!LF_REF_CAS(tail_ptr, tail_cur, tail_next)) {
      LF_PAUSE();
      continue; // Some other core beat us.. try again..
    }

    // At this point, the enqueue is successfull (committed). We still have to bump the
    // tail_idx forward and wake any waiter so all consumers can receive it.
    // NOTE: This CAS can fail if another publish races us. The reason we have others fix up the
    // tail index is that we could crash right here and leave the queue in an inconsistent
    // state. But, if they can do a fixup then the queue is fully crash-proof :-)
    // Thus, we try to do the update and don't check the result: it might fail and that's fine.
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

uint64_t lf_bcast_sub_drops(lf_bcast_sub_t *_sub)
{
  sub_impl_t *sub = (sub_impl_t*)_sub;
  return sub->drops;
}

const void *lf_bcast_sub_consume_begin(lf_bcast_sub_t *_sub, int64_t timeout)
{
  sub_impl_t *sub = (sub_impl_t*)_sub;
  lf_bcast_t *b   = sub->bcast;

  assert(!sub->active);

  while (1) {
    u64 tail_idx = b->tail_idx;
    LF_BARRIER_ACQUIRE();
    if (sub->idx == tail_idx) {
      if (timeout > 0) { /* wait for a wakeup? */
        wait(sub, &timeout);
        continue;
      } else { /* No timeout.. return right away */
        return NULL;
      }
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

    // Recover the buffer pointer
    u64    buf_off = ref.val;
    void * buf     = (char*)b + buf_off;

    sub->active = true;
    return buf;
  }
}

bool lf_bcast_sub_consume_end(lf_bcast_sub_t *_sub)
{
   sub_impl_t *sub = (sub_impl_t*)_sub;
   assert(sub->active);

   // We need to check if the buffer we returned in lf_bcast_sub_consume_begin() could have
   // changed while the user was consuming it. The roll-off procedure is just an increment
   // of the head_idx, so we can verify by simply checking that 'idx >= head_idx'
   u64 head_idx = sub->bcast->head_idx;
   bool valid = sub->idx >= head_idx;
   if (!valid) {
     sub->drops++;
   }

   sub->idx++;
   sub->active = false;
   return valid;
}

void * lf_bcast_buf_acquire(lf_bcast_t *b)
{
  void *elt;

  // Try aquiring from pool or queue until both seem empty.
  while (1) {
    // Try to get a buffer from the pool
    elt = lf_pool_acquire(get_pool(b));
    if (elt) return elt;

    // Pool empty.. chck if the queue has any elements..
    size_t head_idx = b->head_idx;
    size_t tail_idx = b->tail_idx;
    LF_BARRIER_ACQUIRE();
    if (head_idx == tail_idx) return NULL; // Both queue and pool are empty..

    // Try to get a buffer from the queue head
    elt = dequeue_head(b, head_idx);
    if (elt) return elt;
  }
}

void lf_bcast_buf_release(lf_bcast_t *b, void *ptr)
{
  lf_pool_release(get_pool(b), ptr);
}

bool lf_bcast_footprint(size_t depth, size_t elt_sz, size_t elt_align, size_t *_size, size_t *_align)
{
  if (!LF_IS_POW2(depth)) return false;

  size_t pool_size, pool_align;
  if (!lf_pool_footprint(depth, elt_sz, elt_align, &pool_size, &pool_align)) {
    return false;
  }

  size_t size = sizeof(lf_bcast_t);
  size += depth * sizeof(lf_ref_t);
  size = LF_ALIGN_UP(size, pool_align);
  size += pool_size;

  if (_size)  *_size  = size;
  if (_align) *_align = alignof(lf_bcast_t);
  return true;
}

lf_bcast_t * lf_bcast_mem_init(void *mem, size_t depth, size_t elt_sz, size_t elt_align)
{
  /* Sanity check the parameters */
  if (!lf_bcast_footprint(depth, elt_sz, elt_align, NULL, NULL)) {
    return NULL;
  }

  size_t pool_size, pool_align;
  lf_pool_footprint(depth, elt_sz, elt_align, &pool_size, &pool_align);

  size_t size     = sizeof(lf_bcast_t) + depth * sizeof(lf_ref_t);
  size_t pool_off = LF_ALIGN_UP(size, pool_align);
  void * pool_mem = (char*)mem + pool_off;

  lf_bcast_t *b = (lf_bcast_t*)mem;
  b->depth_mask = depth-1;
  b->elt_sz = elt_sz;
  b->head_idx = 1; /* Start from 1 because we use 0 to mean "unused" */
  b->tail_idx = 1; /* Start from 1 because we use 0 to mean "unused" */
  b->pool_off = pool_off;

  memset(b->slots, 0, depth * sizeof(lf_ref_t));

  lf_pool_t *pool = lf_pool_mem_init(pool_mem, depth, elt_sz, elt_align);
  if (!pool) return NULL;
  assert(pool == pool_mem);

  return b;
}

lf_bcast_t * lf_bcast_mem_join(void *mem, size_t depth, size_t elt_sz, size_t elt_align)
{
  /* Sanity check the parameters */
  if (!lf_bcast_footprint(depth, elt_sz, elt_align, NULL, NULL)) {
    return NULL;
  }

  lf_bcast_t *bcast = (lf_bcast_t *)mem;
  if (depth-1 != bcast->depth_mask) return NULL;
  if (elt_sz != bcast->elt_sz) return NULL;

  void * pool_mem = (char*)mem + bcast->pool_off;
  lf_pool_t * pool = lf_pool_mem_join(pool_mem, depth, elt_sz, elt_align);
  if (!pool) return NULL;
  assert(pool == pool_mem);

  return bcast;
}

void lf_bcast_mem_leave(lf_bcast_t *b)
{
  lf_pool_t *pool = get_pool(b);
  lf_pool_mem_leave(pool);
}
