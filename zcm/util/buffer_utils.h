#ifndef __ZCM_BUFFER_UTILS_H__
#define __ZCM_BUFFER_UTILS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// RRR (Bendes): This functionality is already provided via zcm_coretypes.h
//               I don't believe this file should be needed

/** Pull 1 uint8_t out of a uint32_t
    @param var      Value to break
    @param byteNum  Byte number to extract

    @return Extracted byte
*/
static inline uint8_t breakUint32(uint32_t var, int byteNum)
{
    return (uint8_t)((uint32_t)(((var) >> ((byteNum) * 8)) & 0x00FF));
}

/** Break and buffer a uint16_t value - LSB first
    @param pBuf Buffer to insert @val into
    @param val  Value to insert into @pBuf

    @return A pointer to the position immediately following the inserted value
*/
static inline uint8_t* bufferUint16(uint8_t *pBuf, uint16_t val)
{
    *pBuf++ = val & 0xFF;
    *pBuf++ = (val >> 8) & 0xFF;

    return (pBuf);
}

/** Break and buffer a uint32_t value - LSB first
    @param pBuf Buffer to insert @val into
    @param val  Value to insert into @pBuf

    @return A pointer to the position immediately following the inserted value
*/
static inline uint8_t* bufferUint32(uint8_t *pBuf, uint32_t val)
{
    *pBuf++ = breakUint32(val, 0);
    *pBuf++ = breakUint32(val, 1);
    *pBuf++ = breakUint32(val, 2);
    *pBuf++ = breakUint32(val, 3);

    return(pBuf);
}

#ifdef __cplusplus
}
#endif

#endif /* __ZCM_BUFFER_UTILS_H__ */
