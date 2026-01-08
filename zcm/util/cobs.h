#ifndef _ZCM_TRANS_NONBLOCKING_COBS_H
#define _ZCM_TRANS_NONBLOCKING_COBS_H

#include <stdint.h>
#include <stdlib.h>

/* Only define inline for C99 builds or better */
#if (__STDC_VERSION__ >= 199901L) || (__cplusplus)
#define INLINE inline
#else
#define INLINE
#endif

/**
 * @brief   COBS encode \p length bytes from \p src to \p dest
 *
 * @param   dest
 * @param   src
 * @param   length
 *
 * @return  Encoded buffer length in bytes
 *
 * @note    Does not output delimiter byte
 * @note    \p dest must point to output buffer of length
 *          greater than or equal to \p src
 */
static INLINE size_t cobs_encode(uint8_t* dest, const uint8_t* src,
                                 size_t length)
{
    uint8_t* encode = dest;
    uint8_t* codep = encode++;
    uint8_t code = 1;

    for (const uint8_t* byte = src; length--; ++byte) {
        if (*byte)  // not zero, write it
            *encode++ = *byte, ++code;

        if (!*byte || code == 0xff) {  // zero or end of block, restart
            *codep = code;
            code = 1;
            codep = encode;

            if (!*byte || length) ++encode;
        }
    }

    *codep = code;

    return (size_t)(encode - dest);
}

/**
 * @brief   COBS decode \p length bytes from \p src to \p dest
 *
 * @param   dest
 * @param   src
 * @param   length
 *
 * @return  Number of bytes successfully decoded
 *
 * @note    Stops decoding if delimiter byte is found
 */
static INLINE size_t cobs_decode(uint8_t* dest, const uint8_t* src,
                                 size_t length)
{
    const uint8_t* byte = src;
    uint8_t* decode = dest;

    for (uint8_t code = 0xff, block = 0; byte < src + length; --block) {
        if (block) {
            *decode = *byte;
            decode++;
            byte++;
            continue;
        }

        if (code != 0xff) {
            *decode = 0;
            decode++;
        }
        block = code = *byte;
        byte++;

        if (!code) break;  // found delimeter
    }

    return (size_t)(decode - dest);
}

/**
 * @brief   Calculate maximum COBS encoding overhead
 *
 * @param   msgSize Size of the pre-encoded message
 *
 * @return  Maximum number of overhead bytes
 */
static INLINE size_t cobsMaxOverhead(size_t msgSize)
{
    return (msgSize + 253) / 254;
}

#endif /* _ZCM_TRANS_NONBLOCKING_COBS_H */
