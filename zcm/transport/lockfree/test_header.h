#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

static inline i64 wallclock(void)
{
  struct timespec ts[1] = {{}};
  clock_gettime(CLOCK_REALTIME, ts);
  return (i64)ts->tv_sec * 1000000000ul + (i64)ts->tv_nsec;
}

#define FAIL(...) do { fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); abort(); } while(0)

#define REQUIRE(expr) do {\
    if (!(expr)) FAIL("REQUIRE FAILED AT %s:%d WITH '%s'", __FUNCTION__, __LINE__, #expr); \
 } while(0)

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
