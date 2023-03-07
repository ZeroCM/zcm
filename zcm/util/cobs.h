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

/** COBS encode data to buffer
    @param data Pointer to input data to encode
    @param length Number of bytes to encode
    @param buffer Pointer to encoded output buffer

    @return Encoded buffer length in bytes
    @note Does not output delimiter byte
*/
static INLINE size_t
cobsEncode(const void* data, size_t length, uint8_t* buffer) {
    assert(data && buffer);

    uint8_t* encode = buffer;   // Encoded byte pointer
    uint8_t* codep = encode++;  // Output code pointer
    uint8_t code = 1;           // Code value

    for (const uint8_t* byte = (const uint8_t*)data; length--; ++byte) {
        if (*byte)  // Byte not zero, write it
            *encode++ = *byte, ++code;

        if (!*byte || code == 0xff)  // Input is zero or block completed, restart
        {
            *codep = code, code = 1, codep = encode;
            if (!*byte || length)
                ++encode;
        }
    }
    *codep = code;  // Write final code value

    return (size_t)(encode - buffer);
}

/** COBS decode data from buffer
    @param buffer Pointer to encoded input bytes
    @param length Number of bytes to decode
    @param data Pointer to decoded output data

    @return Number of bytes successfully decoded
    @note Stops decoding if delimiter byte is found
*/
static INLINE size_t
cobsDecode(const uint8_t* buffer, size_t length, void* data) {
    assert(buffer && data);

    const uint8_t* byte = buffer;      // Encoded input byte pointer
    uint8_t* decode = (uint8_t*)data;  // Decoded output byte pointer

    for (uint8_t code = 0xff, block = 0; byte < buffer + length; --block) {
        if (block)  // Decode block byte
            *decode++ = *byte++;
        else {
            if (code != 0xff)  // Encoded zero, write it
                *decode++ = 0;
            block = code = *byte++;  // Next block length
            if (!code)               // Delimiter code found
                break;
        }
    }

    return (size_t)(decode - (uint8_t*)data);
}

/** Calculate maximum COBS encoding overhead
    @param msgSize Size of the pre-encoded message

    @return Maximum number of overhead bytes
*/
static INLINE size_t
cobsMaxOverhead(size_t msgSize) {
    return (msgSize + 254 - 1) / 254;  // COBS overhead
}

#endif /* _ZCM_TRANS_NONBLOCKING_COBS_H */
