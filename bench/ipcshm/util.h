#pragma once
#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FAIL(...) do { fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); abort(); } while(0)
#define UNIMPL() FAIL("UNIMPLEMENTED at %s:%d", __FILE__, __LINE__)

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

static inline u64 hash_u64(u64 val)
{
    val ^= val >> 33;
    val *= 0xff51afd7ed558ccdUL;
    val ^= val >> 33;
    val *= 0xc4ceb9fe1a85ec53UL;
    val ^= val >> 33;
    return val;
}

static inline i64 wallclock(void)
{
    struct timespec ts[1] = {{}};
    clock_gettime(CLOCK_REALTIME, ts);
    return (i64)ts->tv_sec * 1000000000ul + (i64)ts->tv_nsec;
}

static inline void hexdump(void *_mem, size_t len)
{
    u8 *mem = (u8*)_mem;

    size_t idx = 0;
    while (idx < len) {
        size_t line_end = MIN(idx+16, len);
        for (; idx < line_end; idx++) {
            printf("%02x ", mem[idx]);
        }
        printf("\n");
    }
}
