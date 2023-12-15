#include "lf_queue.h"
#include "lf_util.h"

/* TODO: Explain the design of this queue */

struct __attribute__((aligned(CACHE_LINE_SZ))) lf_queue
{
  u64  depth_mask;
  u64  head_idx;
  u64  tail_idx;
  char _pad_2[CACHE_LINE_SZ - 3*sizeof(u64)];

  lf_ref_t slots[];
};
static_assert(sizeof(lf_queue_t) == CACHE_LINE_SZ, "");
static_assert(alignof(lf_queue_t) == CACHE_LINE_SZ, "");

lf_queue_t * lf_queue_new(size_t depth)
{
  size_t mem_sz, mem_align;
  lf_queue_footprint(depth, &mem_sz, &mem_align);

  void *mem = NULL;
  int ret = posix_memalign(&mem, mem_align, mem_sz);
  if (ret != 0) return NULL;

  lf_queue_t *queue = lf_queue_mem_init(mem, depth);
  if (!queue) {
    free(mem);
    return NULL;
  }

  return queue;
}

void lf_queue_delete(lf_queue_t *q)
{
  free(q);
}

bool lf_queue_enqueue(lf_queue_t *q, uint64_t val)
{
  while (1) {
    u64        head_idx = q->head_idx;
    u64        tail_idx = q->tail_idx;
    lf_ref_t * tail_ptr = &q->slots[tail_idx & q->depth_mask];
    lf_ref_t   tail_cur = *tail_ptr;

    // Stale tail pointer? Try to advance it..
    if (tail_cur.tag == tail_idx) {
      LF_U64_CAS(&q->tail_idx, tail_idx, tail_idx+1);
      LF_PAUSE();
      continue;
    }

    // Slot currently used?
    if (head_idx <= tail_cur.tag) return false; // Full!

    // Otherwise, try to append the tail
    lf_ref_t tail_next = LF_REF_MAKE(tail_idx, val);
    if (!LF_REF_CAS(tail_ptr, tail_cur, tail_next)) {
      continue;
      LF_PAUSE();
    }

    // Success, try to update the tail. If we fail, it's okay.
    LF_U64_CAS(&q->tail_idx, tail_idx, tail_idx+1);
    return true;
  }
}

bool lf_queue_dequeue(lf_queue_t *q, uint64_t *_val)
{
  while (1) {
    u64        head_idx = q->head_idx;
    u64        tail_idx = q->tail_idx;
    lf_ref_t * head_ptr = &q->slots[head_idx & q->depth_mask];
    lf_ref_t   head_cur = *head_ptr;

    // When head reaches tail, we either have an empty queue or a stale tail
    if (head_idx == tail_idx) {
      if (head_cur.tag != head_idx) return false; // Empty!
      // Head seems valid, but the tail is stale.. try to update it
      LF_U64_CAS(&q->tail_idx, tail_idx, tail_idx);
      LF_PAUSE();
      continue;
    }

    // Try to pop head
    // IMPORTANT: On success, the head node is considered "deallocated"
    // which means it is invalid for us to access it in any way. Another
    // operation may reuse it before our access and we could read stale data
    u64 val = head_cur.val;
    if (!LF_U64_CAS(&q->head_idx, head_idx, head_idx+1)) {
      LF_PAUSE();
      continue;
    }

    // Success!
    *_val = val;
    return true;
  }
}

void lf_queue_footprint(size_t depth, size_t *_size, size_t *_align)
{
  if (_size)  *_size  = sizeof(lf_queue_t) + depth * sizeof(lf_ref_t);
  if (_align) *_align = alignof(lf_queue_t);
}

lf_queue_t *lf_queue_mem_init(void *mem, size_t depth)
{
  if (!LF_IS_POW2(depth)) return NULL;

  lf_queue_t *queue = (lf_queue_t *)mem;
  queue->depth_mask = depth-1;
  queue->head_idx = 1; /* Start from 1 because we use 0 to mean "unused" */
  queue->tail_idx = 1; /* Start from 1 because we use 0 to mean "unused" */

  memset(queue->slots, 0, depth * sizeof(lf_ref_t));

  return queue;
}

lf_queue_t *lf_queue_mem_join(void *mem, size_t depth)
{
  lf_queue_t *queue = (lf_queue_t *)mem;
  if (depth-1 != queue->depth_mask) return NULL;
  return queue;
}

void lf_queue_mem_leave(lf_queue_t *lf_queue)
{
  /* no-op at the moment */
}
