#ifndef _ZCM_TRANS_NONBLOCKING_SERIAL_FLETCHER_H
#define _ZCM_TRANS_NONBLOCKING_SERIAL_FLETCHER_H

#include <stdint.h>
#include <stdlib.h>

static inline uint16_t fletcherUpdate(uint8_t b, uint16_t prevSum)
{
    uint16_t sumHigh = (prevSum >> 8) & 0xff;
    uint16_t sumLow  =  prevSum       & 0xff;
    sumHigh += sumLow += b;

    sumLow  = (sumLow  & 0xff) + (sumLow  >> 8);
    sumHigh = (sumHigh & 0xff) + (sumHigh >> 8);

    // Note: double reduction to ensure no overflow after first
    sumLow  = (sumLow  & 0xff) + (sumLow  >> 8);
    sumHigh = (sumHigh & 0xff) + (sumHigh >> 8);

    return (sumHigh << 8) | sumLow;
}

/*!
 * @brief      Calculate Fletcher-16 checksum
 *
 * @param      data - pointed to array of bytes to calculate checksum for
 * @param      len  - length of byte array
 */
static inline uint16_t fletcher16(const uint8_t* data, size_t len)
{
    uint32_t c0, c1;

    /* Found by solving for c1 overflow: */
    /* n > 0 and n * (n+1) / 2 * (2^8-1) < (2^32-1). */
    for (c0 = c1 = 0; len > 0;) {
        size_t blocklen = len;
        if (blocklen > 5802) {
            blocklen = 5802;
        }
        len -= blocklen;
        do {
            c0 = c0 + *data++;
            c1 = c1 + c0;
        } while (--blocklen);
        c0 = c0 % 255;
        c1 = c1 % 255;
    }
    return (c1 << 8 | c0);
}

#endif /* _ZCM_TRANS_NONBLOCKING_FLETCHER_H */
