#ifndef _ZCM_LIB_INLINE_H
#define _ZCM_LIB_INLINE_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void *zcm_malloc(size_t sz)
{
    if (sz) return malloc(sz);
    return NULL;
}

static inline void zcm_free(void* mem)
{
    free(mem);
}

typedef struct ___zcm_hash_ptr __zcm_hash_ptr;
struct ___zcm_hash_ptr
{
    const __zcm_hash_ptr *parent;
    void *v;
};

/**
 * BOOLEAN
 */
#define __boolean_hash_recursive __int8_t_hash_recursive
#define __boolean_decode_array_cleanup __int8_t_decode_array_cleanup
#define __boolean_encoded_array_size __int8_t_encoded_array_size
#define __boolean_encode_array __int8_t_encode_array
#define __boolean_decode_array __int8_t_decode_array
#define __boolean_encode_little_endian_array __int8_t_encode_little_endian_array
#define __boolean_decode_little_endian_array __int8_t_decode_little_endian_array
#define __boolean_clone_array __int8_t_clone_array

/**
 * BYTE
 */
#define __byte_hash_recursive(p) 0
#define __byte_decode_array_cleanup(p, sz) {}

static inline int __byte_encoded_array_size(const uint8_t *p, int elements)
{
    (void)p;
    return sizeof(uint8_t) * elements;
}

static inline int __byte_encode_array(void *_buf, int offset, int maxlen, const uint8_t *p, int elements)
{
    if (maxlen < elements)
        return -1;

    uint8_t *buf = (uint8_t*) _buf;
    memcpy(&buf[offset], p, elements);

    return elements;
}

static inline int __byte_decode_array(const void *_buf, int offset, int maxlen, uint8_t *p, int elements)
{
    if (maxlen < elements)
        return -1;

    uint8_t *buf = (uint8_t*) _buf;
    memcpy(p, &buf[offset], elements);

    return elements;
}

static inline int __byte_encode_little_endian_array(void *_buf, int offset, int maxlen, const uint8_t *p, int elements)
{
    return __byte_encode_array(_buf, offset, maxlen, p, elements);
}

static inline int __byte_decode_little_endian_array(const void *_buf, int offset, int maxlen, uint8_t *p, int elements)
{
    return __byte_decode_array(_buf, offset, maxlen, p, elements);
}

static inline int __byte_clone_array(const uint8_t *p, uint8_t *q, int elements)
{
    int n = elements * sizeof(uint8_t);
    memcpy(q, p, n);
    return n;
}
/**
 * INT8_T
 */
#define __int8_t_hash_recursive(p) 0
#define __int8_t_decode_array_cleanup(p, sz) {}

static inline int __int8_t_encoded_array_size(const int8_t *p, int elements)
{
    (void)p;
    return sizeof(int8_t) * elements;
}

static inline int __int8_t_encode_array(void *_buf, int offset, int maxlen, const int8_t *p, int elements)
{
    if (maxlen < elements)
        return -1;

    int8_t *buf = (int8_t*) _buf;
    memcpy(&buf[offset], p, elements);

    return elements;
}

static inline int __int8_t_decode_array(const void *_buf, int offset, int maxlen, int8_t *p, int elements)
{
    if (maxlen < elements)
        return -1;

    int8_t *buf = (int8_t*) _buf;
    memcpy(p, &buf[offset], elements);

    return elements;
}

static inline int __int8_t_encode_little_endian_array(void *_buf, int offset, int maxlen, const int8_t *p, int elements)
{
    return __int8_t_encode_array(_buf, offset, maxlen, p, elements);
}

static inline int __int8_t_decode_little_endian_array(const void *_buf, int offset, int maxlen, int8_t *p, int elements)
{
    return __int8_t_decode_array(_buf, offset, maxlen, p, elements);
}

static inline int __int8_t_clone_array(const int8_t *p, int8_t *q, int elements)
{
    int n = elements * sizeof(int8_t);
    memcpy(q, p, n);
    return n;
}

/**
 * INT16_T
 */
#define __int16_t_hash_recursive(p) 0
#define __int16_t_decode_array_cleanup(p, sz) {}

static inline int __int16_t_encoded_array_size(const int16_t *p, int elements)
{
    (void)p;
    return sizeof(int16_t) * elements;
}

static inline int __int16_t_encode_array(void *_buf, int offset, int maxlen, const int16_t *p, int elements)
{
    int total_size = sizeof(int16_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    const uint16_t *unsigned_p = (uint16_t*)p;
    for (element = 0; element < elements; element++) {
        uint16_t v = unsigned_p[element];
        buf[pos++] = (v>>8) & 0xff;
        buf[pos++] = (v & 0xff);
    }

    return total_size;
}

static inline int __int16_t_decode_array(const void *_buf, int offset, int maxlen, int16_t *p, int elements)
{
    int total_size = sizeof(int16_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; element++) {
        p[element] = (buf[pos]<<8) + buf[pos+1];
        pos+=2;
    }

    return total_size;
}

static inline int __int16_t_encode_little_endian_array(void *_buf, int offset, int maxlen, const int16_t *p, int elements)
{
    int total_size = sizeof(int16_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    const uint16_t *unsigned_p = (uint16_t*)p;
    for (element = 0; element < elements; element++) {
        uint16_t v = unsigned_p[element];
        buf[pos++] = (v & 0xff);
        buf[pos++] = (v>>8) & 0xff;
    }

    return total_size;
}

static inline int __int16_t_decode_little_endian_array(const void *_buf, int offset, int maxlen, int16_t *p, int elements)
{
    int total_size = sizeof(int16_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; element++) {
        p[element] = (buf[pos+1]<<8) + buf[pos];
        pos+=2;
    }

    return total_size;
}

static inline int __int16_t_clone_array(const int16_t *p, int16_t *q, int elements)
{
    int n = elements * sizeof(int16_t);
    memcpy(q, p, n);
    return n;
}

/**
 * INT32_T
 */
#define __int32_t_hash_recursive(p) 0
#define __int32_t_decode_array_cleanup(p, sz) {}

static inline int __int32_t_encoded_array_size(const int32_t *p, int elements)
{
    (void)p;
    return sizeof(int32_t) * elements;
}

static inline int __int32_t_encode_array(void *_buf, int offset, int maxlen, const int32_t *p, int elements)
{
    int total_size = sizeof(int32_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    const uint32_t* unsigned_p = (uint32_t*)p;
    for (element = 0; element < elements; element++) {
        uint32_t v = unsigned_p[element];
        buf[pos++] = (v>>24)&0xff;
        buf[pos++] = (v>>16)&0xff;
        buf[pos++] = (v>>8)&0xff;
        buf[pos++] = (v & 0xff);
    }

    return total_size;
}

static inline int __int32_t_decode_array(const void *_buf, int offset, int maxlen, int32_t *p, int elements)
{
    int total_size = sizeof(int32_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; element++) {
        p[element] = (((uint32_t)buf[pos+0])<<24) +
                     (((uint32_t)buf[pos+1])<<16) +
                     (((uint32_t)buf[pos+2])<<8) +
                      ((uint32_t)buf[pos+3]);
        pos+=4;
    }

    return total_size;
}

static inline int __int32_t_encode_little_endian_array(void *_buf, int offset, int maxlen, const int32_t *p, int elements)
{
    int total_size = sizeof(int32_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    const uint32_t* unsigned_p = (uint32_t*)p;
    for (element = 0; element < elements; element++) {
        uint32_t v = unsigned_p[element];
        buf[pos++] = (v & 0xff);
        buf[pos++] = (v>>8)&0xff;
        buf[pos++] = (v>>16)&0xff;
        buf[pos++] = (v>>24)&0xff;
    }

    return total_size;
}

static inline int __int32_t_decode_little_endian_array(const void *_buf, int offset, int maxlen, int32_t *p, int elements)
{
    int total_size = sizeof(int32_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; element++) {
        p[element] = (((uint32_t)buf[pos+3])<<24) +
                      (((uint32_t)buf[pos+2])<<16) +
                      (((uint32_t)buf[pos+1])<<8) +
                       ((uint32_t)buf[pos+0]);
        pos+=4;
    }

    return total_size;
}

static inline int __int32_t_clone_array(const int32_t *p, int32_t *q, int elements)
{
    int n = elements * sizeof(int32_t);
    memcpy(q, p, n);
    return n;
}

/**
 * INT64_T
 */
#define __int64_t_hash_recursive(p) 0
#define __int64_t_decode_array_cleanup(p, sz) {}

static inline int __int64_t_encoded_array_size(const int64_t *p, int elements)
{
    (void)p;
    return sizeof(int64_t) * elements;
}

static inline int __int64_t_encode_array(void *_buf, int offset, int maxlen, const int64_t *p, int elements)
{
    int total_size = sizeof(int64_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    const uint64_t* unsigned_p = (uint64_t*)p;
    for (element = 0; element < elements; element++) {
        uint64_t v = unsigned_p[element];
        buf[pos++] = (v>>56)&0xff;
        buf[pos++] = (v>>48)&0xff;
        buf[pos++] = (v>>40)&0xff;
        buf[pos++] = (v>>32)&0xff;
        buf[pos++] = (v>>24)&0xff;
        buf[pos++] = (v>>16)&0xff;
        buf[pos++] = (v>>8)&0xff;
        buf[pos++] = (v & 0xff);
    }

    return total_size;
}

static inline int __int64_t_decode_array(const void *_buf, int offset, int maxlen, int64_t *p, int elements)
{
    int total_size = sizeof(int64_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; element++) {
        uint64_t a = (((uint32_t)buf[pos+0])<<24) +
                     (((uint32_t)buf[pos+1])<<16) +
                     (((uint32_t)buf[pos+2])<<8) +
                      ((uint32_t)buf[pos+3]);
        pos+=4;
        uint64_t b = (((uint32_t)buf[pos+0])<<24) +
                     (((uint32_t)buf[pos+1])<<16) +
                     (((uint32_t)buf[pos+2])<<8) +
                      ((uint32_t)buf[pos+3]);
        pos+=4;
        p[element] = (a<<32) + (b&0xffffffff);
    }

    return total_size;
}

static inline int __int64_t_encode_little_endian_array(void *_buf, int offset, int maxlen, const int64_t *p, int elements)
{
    int total_size = sizeof(int64_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    const uint64_t* unsigned_p = (uint64_t*)p;
    for (element = 0; element < elements; element++) {
        uint64_t v = unsigned_p[element];
        buf[pos++] = (v & 0xff);
        buf[pos++] = (v>>8)&0xff;
        buf[pos++] = (v>>16)&0xff;
        buf[pos++] = (v>>24)&0xff;
        buf[pos++] = (v>>32)&0xff;
        buf[pos++] = (v>>40)&0xff;
        buf[pos++] = (v>>48)&0xff;
        buf[pos++] = (v>>56)&0xff;
    }

    return total_size;
}

static inline int __int64_t_decode_little_endian_array(const void *_buf, int offset, int maxlen, int64_t *p, int elements)
{
    int total_size = sizeof(int64_t) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; element++) {
        uint64_t b = (((uint32_t)buf[pos+3])<<24) +
                     (((uint32_t)buf[pos+2])<<16) +
                     (((uint32_t)buf[pos+1])<<8) +
                      ((uint32_t)buf[pos+0]);
        pos+=4;
        uint64_t a = (((uint32_t)buf[pos+3])<<24) +
                     (((uint32_t)buf[pos+2])<<16) +
                     (((uint32_t)buf[pos+1])<<8) +
                      ((uint32_t)buf[pos+0]);
        pos+=4;
        p[element] = (a<<32) + (b&0xffffffff);
    }

    return total_size;
}

static inline int __int64_t_clone_array(const int64_t *p, int64_t *q, int elements)
{
    int n = elements * sizeof(int64_t);
    memcpy(q, p, n);
    return n;
}

/**
 * FLOAT
 */
typedef union __zcm__float_uint32_t {
    float flt;
    uint32_t uint;
} __zcm__float_uint32_t;

#define __float_hash_recursive(p) 0
#define __float_decode_array_cleanup(p, sz) {}

static inline int __float_encoded_array_size(const float *p, int elements)
{
    (void)p;
    return sizeof(float) * elements;
}

static inline int __float_encode_array(void *_buf, int offset, int maxlen, const float *p, int elements)
{
    int total_size = sizeof(float) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;
    __zcm__float_uint32_t tmp;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; ++element) {
        tmp.flt = p[element];
        buf[pos++] = (tmp.uint >> 24) & 0xff;
        buf[pos++] = (tmp.uint >> 16) & 0xff;
        buf[pos++] = (tmp.uint >>  8) & 0xff;
        buf[pos++] = (tmp.uint      ) & 0xff;
    }

    return total_size;
}

static inline int __float_decode_array(const void *_buf, int offset, int maxlen, float *p, int elements)
{
    int total_size = sizeof(float) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;
    __zcm__float_uint32_t tmp;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; ++element) {
        tmp.uint = (((uint32_t)buf[pos + 0]) << 24) |
                   (((uint32_t)buf[pos + 1]) << 16) |
                   (((uint32_t)buf[pos + 2]) <<  8) |
                    ((uint32_t)buf[pos + 3]);
        p[element] = tmp.flt;
        pos += 4;
    }

    return total_size;
}

static inline int __float_encode_little_endian_array(void *_buf, int offset, int maxlen, const float *p, int elements)
{
    int total_size = sizeof(float) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;
    __zcm__float_uint32_t tmp;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; ++element) {
        tmp.flt = p[element];
        buf[pos++] = (tmp.uint      ) & 0xff;
        buf[pos++] = (tmp.uint >>  8) & 0xff;
        buf[pos++] = (tmp.uint >> 16) & 0xff;
        buf[pos++] = (tmp.uint >> 24) & 0xff;
    }

    return total_size;
}

static inline int __float_decode_little_endian_array(const void *_buf, int offset, int maxlen, float *p, int elements)
{
    int total_size = sizeof(float) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;
    __zcm__float_uint32_t tmp;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; ++element) {
        tmp.uint = (((uint32_t)buf[pos + 3]) << 24) |
                   (((uint32_t)buf[pos + 2]) << 16) |
                   (((uint32_t)buf[pos + 1]) <<  8) |
                    ((uint32_t)buf[pos + 0]);
        p[element] = tmp.flt;
        pos += 4;
    }

    return total_size;
}

static inline int __float_clone_array(const float *p, float *q, int elements)
{
    int n = elements * sizeof(float);
    memcpy(q, p, n);
    return n;
}

/**
 * DOUBLE
 */
typedef union __zcm__double_uint64_t {
    double dbl;
    uint64_t uint;
} __zcm__double_uint64_t;

#define __double_hash_recursive(p) 0
#define __double_decode_array_cleanup(p, sz) {}

static inline int __double_encoded_array_size(const double *p, int elements)
{
    (void)p;
    return sizeof(double) * elements;
}

static inline int __double_encode_array(void *_buf, int offset, int maxlen, const double *p, int elements)
{
    int total_size = sizeof(double) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;
    __zcm__double_uint64_t tmp;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; ++element) {
        tmp.dbl = p[element];
        buf[pos++] = (tmp.uint >> 56) & 0xff;
        buf[pos++] = (tmp.uint >> 48) & 0xff;
        buf[pos++] = (tmp.uint >> 40) & 0xff;
        buf[pos++] = (tmp.uint >> 32) & 0xff;
        buf[pos++] = (tmp.uint >> 24) & 0xff;
        buf[pos++] = (tmp.uint >> 16) & 0xff;
        buf[pos++] = (tmp.uint >>  8) & 0xff;
        buf[pos++] = (tmp.uint      ) & 0xff;
    }

    return total_size;
}

static inline int __double_decode_array(const void *_buf, int offset, int maxlen, double *p, int elements)
{
    int total_size = sizeof(double) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;
    __zcm__double_uint64_t tmp;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; ++element) {
        uint64_t a = (((uint32_t) buf[pos + 0]) << 24) +
                     (((uint32_t) buf[pos + 1]) << 16) +
                     (((uint32_t) buf[pos + 2]) <<  8) +
                      ((uint32_t) buf[pos + 3]);
        pos += 4;
        uint64_t b = (((uint32_t) buf[pos + 0]) << 24) +
                     (((uint32_t) buf[pos + 1]) << 16) +
                     (((uint32_t) buf[pos + 2]) <<  8) +
                      ((uint32_t) buf[pos + 3]);
        pos += 4;
        tmp.uint = (a << 32) + (b & 0xffffffff);
        p[element] = tmp.dbl;
    }

    return total_size;
}

static inline int __double_encode_little_endian_array(void *_buf, int offset, int maxlen, const double *p, int elements)
{
    int total_size = sizeof(double) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;
    __zcm__double_uint64_t tmp;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; ++element) {
        tmp.dbl = p[element];
        buf[pos++] = (tmp.uint      ) & 0xff;
        buf[pos++] = (tmp.uint >>  8) & 0xff;
        buf[pos++] = (tmp.uint >> 16) & 0xff;
        buf[pos++] = (tmp.uint >> 24) & 0xff;
        buf[pos++] = (tmp.uint >> 32) & 0xff;
        buf[pos++] = (tmp.uint >> 40) & 0xff;
        buf[pos++] = (tmp.uint >> 48) & 0xff;
        buf[pos++] = (tmp.uint >> 56) & 0xff;
    }

    return total_size;
}

static inline int __double_decode_little_endian_array(const void *_buf, int offset, int maxlen, double *p, int elements)
{
    int total_size = sizeof(double) * elements;
    uint8_t *buf = (uint8_t*) _buf;
    int pos = offset;
    int element;
    __zcm__double_uint64_t tmp;

    if (maxlen < total_size)
        return -1;

    for (element = 0; element < elements; ++element) {
        uint64_t b = (((uint32_t)buf[pos + 3]) << 24) +
                     (((uint32_t)buf[pos + 2]) << 16) +
                     (((uint32_t)buf[pos + 1]) <<  8) +
                      ((uint32_t)buf[pos + 0]);
        pos += 4;
        uint64_t a = (((uint32_t)buf[pos + 3]) << 24) +
                     (((uint32_t)buf[pos + 2]) << 16) +
                     (((uint32_t)buf[pos + 1]) <<  8) +
                      ((uint32_t)buf[pos + 0]);
        pos += 4;
        tmp.uint = (a << 32) + (b & 0xffffffff);
        p[element] = tmp.dbl;
    }

    return total_size;
}

static inline int __double_clone_array(const double *p, double *q, int elements)
{
    int n = elements * sizeof(double);
    memcpy(q, p, n);
    return n;
}

/**
 * STRING
 */
#define __string_hash_recursive(p) 0

static inline int __string_decode_array_cleanup(char **s, int elements)
{
    int element;
    for (element = 0; element < elements; element++)
        free(s[element]);
    return 0;
}

// XXX (Bendes) Not sure why "const char * const * p" doesn't work
static inline int __string_encoded_array_size(char * const *s, int elements)
{
    int size = 0;
    int element;
    for (element = 0; element < elements; element++)
        size += 4 + strlen(s[element]) + 1;

    return size;
}

// XXX (Bendes) Not sure why "const char * const * p" doesn't work
static inline int __string_encode_array(void *_buf, int offset, int maxlen, char * const *p, int elements)
{
    int pos = 0, thislen;
    int element;

    for (element = 0; element < elements; element++) {
        int32_t length = strlen(p[element]) + 1; // length includes \0

        thislen = __int32_t_encode_array(_buf, offset + pos, maxlen - pos, &length, 1);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __int8_t_encode_array(_buf, offset + pos, maxlen - pos, (int8_t*) p[element], length);
        if (thislen < 0) return thislen; else pos += thislen;
    }

    return pos;
}

static inline int __string_decode_array(const void *_buf, int offset, int maxlen, char **p, int elements)
{
    int pos = 0, thislen;
    int element;

    for (element = 0; element < elements; element++) {
        int32_t length;

        // read length including \0
        thislen = __int32_t_decode_array(_buf, offset + pos, maxlen - pos, &length, 1);
        if (thislen < 0) return thislen; else pos += thislen;

        p[element] = (char*) zcm_malloc(length);
        thislen = __int8_t_decode_array(_buf, offset + pos, maxlen - pos, (int8_t*) p[element], length);
        if (thislen < 0) return thislen; else pos += thislen;
    }

    return pos;
}

// XXX (Bendes) Not sure why "const char * const * p" doesn't work
static inline int __string_encode_little_endian_array(void *_buf, int offset, int maxlen, char * const *p, int elements)
{
    int pos = 0, thislen;
    int element;

    for (element = 0; element < elements; element++) {
        int32_t length = strlen(p[element]) + 1; // length includes \0

        thislen = __int32_t_encode_little_endian_array(_buf, offset + pos, maxlen - pos, &length, 1);
        if (thislen < 0) return thislen; else pos += thislen;

        thislen = __int8_t_encode_little_endian_array(_buf, offset + pos, maxlen - pos, (int8_t*) p[element], length);
        if (thislen < 0) return thislen; else pos += thislen;
    }

    return pos;
}

static inline int __string_decode_little_endian_array(const void *_buf, int offset, int maxlen, char **p, int elements)
{
    int pos = 0, thislen;
    int element;

    for (element = 0; element < elements; element++) {
        int32_t length;

        // read length including \0
        thislen = __int32_t_decode_little_endian_array(_buf, offset + pos, maxlen - pos, &length, 1);
        if (thislen < 0) return thislen; else pos += thislen;

        p[element] = (char*) zcm_malloc(length);
        thislen = __int8_t_decode_little_endian_array(_buf, offset + pos, maxlen - pos, (int8_t*) p[element], length);
        if (thislen < 0) return thislen; else pos += thislen;
    }

    return pos;
}

// XXX (Bendes) Not sure why "const char * const * p" doesn't work
static inline int __string_clone_array(char * const *p, char **q, int elements)
{
    int ret = 0;
    int element;
    for (element = 0; element < elements; element++) {
        // because strdup is not C99
        size_t len = strlen(p[element]) + 1;
        ret += len;
        q[element] = (char*) zcm_malloc (len);
        memcpy (q[element], p[element], len);
    }
    return ret;
}

/**
 * Describes the type of a single field in an ZCM message.
 */
typedef enum {
    ZCM_FIELD_INT8_T,
    ZCM_FIELD_INT16_T,
    ZCM_FIELD_INT32_T,
    ZCM_FIELD_INT64_T,
    ZCM_FIELD_BYTE,
    ZCM_FIELD_FLOAT,
    ZCM_FIELD_DOUBLE,
    ZCM_FIELD_STRING,
    ZCM_FIELD_BOOLEAN,
    ZCM_FIELD_USER_TYPE
} zcm_field_type_t;

#define ZCM_TYPE_FIELD_MAX_DIM 50

/**
 * Describes a single zcmtype field's datatype and array dimmensions
 */
typedef struct _zcm_field_t zcm_field_t;
struct _zcm_field_t
{
    /**
     * name of the field
     */
    const char *name;

    /**
     * datatype of the field
     **/
    zcm_field_type_t type;

    /**
     * datatype of the field (in string format)
     * this should be the same as in the zcm type decription file
     */
    const char *typestr;

    /**
     * number of array dimensions
     * if the field is scalar, num_dim should equal 0
     */
    int num_dim;

    /**
     * the size of each dimension. Valid on [0:num_dim-1].
     */
    int32_t dim_size[ZCM_TYPE_FIELD_MAX_DIM];

    /**
     * a boolean describing whether the dimension is
     * variable. Valid on [0:num_dim-1].
     */
    int8_t  dim_is_variable[ZCM_TYPE_FIELD_MAX_DIM];

    /**
     * a data pointer to the start of this field
     */
    void *data;
};

typedef int     (*zcm_encode_t)(void *buf, int offset, int maxlen, const void *p);
typedef int     (*zcm_decode_t)(const void *buf, int offset, int maxlen, void *p);
typedef int     (*zcm_decode_cleanup_t)(void *p);
typedef int     (*zcm_encoded_size_t)(const void *p);
typedef int     (*zcm_struct_size_t)(void);
typedef int     (*zcm_num_fields_t)(void);
typedef int     (*zcm_get_field_t)(const void *p, int i, zcm_field_t *f);
typedef int64_t (*zcm_get_hash_t)(void);

/**
 * Describes an zcmtype info, enabling introspection
 */
typedef struct _zcm_type_info_t zcm_type_info_t;
struct _zcm_type_info_t
{
    zcm_encode_t          encode;
    zcm_decode_t          decode;
    zcm_decode_cleanup_t  decode_cleanup;
    zcm_encoded_size_t    encoded_size;
    zcm_struct_size_t     struct_size;
    zcm_num_fields_t      num_fields;
    zcm_get_field_t       get_field;
    zcm_get_hash_t        get_hash;

};

#ifdef __cplusplus
}
#endif

#endif // _ZCM_LIB_INLINE_H
