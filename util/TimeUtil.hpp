#pragma once
#include "util/Types.hpp"
#include <sys/time.h>

#ifdef MONOTONIC_CLOCK
#include <time.h>
#endif

namespace TimeUtil {
static u64 utime()
{
#ifdef MONOTONIC_CLOCK
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts); // or CLOCK_MONOTONIC
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}
} // namespace TimeUtil
