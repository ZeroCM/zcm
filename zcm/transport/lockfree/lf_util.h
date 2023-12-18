#pragma once
#include "lf_machine_assumptions.h"
#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define CACHE_LINE_SZ 128

typedef uint8_t           u8;
typedef uint16_t          u16;
typedef uint32_t          u32;
typedef uint64_t          u64;
typedef unsigned __int128 u128;
typedef int8_t            i8;
typedef int16_t           i16;
typedef int32_t           i32;
typedef int64_t           i64;
typedef __int128          i128;

static inline i64 wallclock(void)
{
  struct timespec ts[1] = {{}};
  clock_gettime(CLOCK_REALTIME, ts);
  return (i64)ts->tv_sec * 1000000000ul + (i64)ts->tv_nsec;
}

typedef struct lf_ref lf_ref_t;
struct lf_ref
{
  union {
    struct {
      u64 val;
      u64 tag;
    };
    unsigned __int128 u128;
  };
};
static_assert(sizeof(lf_ref_t) == sizeof(unsigned __int128), "");
static_assert(alignof(lf_ref_t) == alignof(unsigned __int128), "");

#define LF_REF_MAKE(_tag, _val)      ({ lf_ref_t r; r.val = _val; r.tag = _tag; r; })
#define LF_REF_NULL                  ({ lf_ref_t r = {}; r; })
#define LF_REF_IS_NULL(ref)          ((ref).u128 == 0)
#define LF_REF_EQUAL(a, b)           ((a).u128 == (b).u128)

#define LF_REF_CAS(ptr, last, next)  _impl_lf_ref_cas(ptr, last, next)
#define LF_U64_CAS(ptr, last, next)  _impl_lf_u64_cas(ptr, last, next)

#define LF_ATOMIC_INC(ptr)           __sync_add_and_fetch(ptr, 1);
#define LF_ATOMIC_LOAD_ACQUIRE(ptr)  ({ typeof(*ptr) ret; __atomic_load(ptr, &ret, __ATOMIC_ACQUIRE); ret; })
#define LF_BARRIER_ACQUIRE()         __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define LF_BARRIER_RELEASE()         __atomic_thread_fence(__ATOMIC_RELEASE)
#define LF_BARRIER_FULL()            __atomic_thread_fence(__ATOMIC_SEQ_CST)

#define LF_ALIGN_UP(s, a)            (((s)+((a)-1))&~((a)-1))
#define LF_IS_POW2(s)                ((!!(s))&(!((s)&((s)-1))))

#if defined(__arm__)
# define LF_PAUSE() __yield()
#elif defined(__x86_64__)
# define LF_PAUSE() __asm__ __volatile__("pause" ::: "memory")
#else
# define LF_PAUSE()
#endif

static inline __attribute__((always_inline)) bool _impl_lf_ref_cas(lf_ref_t *ptr, lf_ref_t prev, lf_ref_t next)
{
  return __sync_bool_compare_and_swap(&ptr->u128, prev.u128, next.u128);
}

static inline __attribute__((always_inline)) bool _impl_lf_u64_cas(u64 *ptr, u64 prev, u64 next)
{
  return __sync_bool_compare_and_swap(ptr, prev, next);
}
