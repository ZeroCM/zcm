#ifndef __ZCM_IOUTILS_H__
#define __ZCM_IOUTILS_H__

#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <arpa/inet.h>
#else
#include <winsock2.h>
#endif

#include "zcm/zcm_coretypes.h"
#include "zcm/eventlog.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int fwrite32(FILE *f, int32_t v)
{
    v = htonl(v);
    if (fwrite(&v, 4, 1, f) == 1) return 0; else return -1;
}

static inline int fwrite64(FILE *f, int64_t v64)
{
    int32_t v = ((uint64_t)v64)>>32;
    if (0 != fwrite32(f, v)) return -1;
    v = v64 & 0xffffffff;
    return fwrite32(f, v);
}

static inline int readn(zcm_eventlog_t *l, uint8_t *v, size_t n)
{
    if (l->offset > l->size) return -1;
    if (__int8_t_decode_array(l->data, l->offset, l->size - l->offset, (int8_t*)v, n) != n)
        return -1;
    l->offset += n;
    return 0;
}

static inline int read32(zcm_eventlog_t *l, int32_t *v)
{
    if (l->offset > l->size) return -1;
    if (__int32_t_decode_array(l->data, l->offset, l->size - l->offset, v, 1) != 4)
        return -1;
    l->offset += 4;
    return 0;
}

static inline int read64(zcm_eventlog_t *l, int64_t *v)
{
    if (l->offset > l->size) return -1;
    if (__int64_t_decode_array(l->data, l->offset, l->size - l->offset, v, 1) != 8)
        return -1;
    l->offset += 8;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
