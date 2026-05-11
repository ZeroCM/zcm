#ifndef _ZCM_TRANS_NONBLOCKING_SERIAL_FLETCHER_H
#define _ZCM_TRANS_NONBLOCKING_SERIAL_FLETCHER_H

#include <stdint.h>

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

static inline uint16_t fletcher16(const uint8_t* data, size_t len, uint16_t prevSum)
{
    size_t   i;
    for (i = 0; i < len; ++i) prevSum = fletcherUpdate(data[i], prevSum);
    return prevSum;
}

#endif /* _ZCM_TRANS_NONBLOCKING_FLETCHER_H */
