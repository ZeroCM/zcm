#ifndef __ZCM_BYTEORDER_H__
#define __ZCM_BYTEORDER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t zcm_read_u16_be(const uint8_t* p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static inline uint32_t zcm_read_u32_be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static inline void zcm_write_u16_be(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xff);
    p[1] = (uint8_t)(v & 0xff);
}

static inline void zcm_write_u32_be(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xff);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >> 8)  & 0xff);
    p[3] = (uint8_t)(v & 0xff);
}

#ifdef __cplusplus
}
#endif

#endif /* __ZCM_BYTEORDER_H__ */
