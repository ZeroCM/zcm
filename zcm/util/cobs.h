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

// RRR (Bendes): Try to match style of the repo you're working in.
//               This repo has curly braces for functions alone on the next line.
//               function definitions all on one line. Line length attempted to
//               not exceed 80 characters, but definitely not to exceed 90
//               characters

// RRR (Bendes): C style is that destintations go first in function args.
//               Example: cobsEncode(dest, src, len)
/** COBS encode data to buffer
    @param data Pointer to input data to encode
    @param length Number of bytes to encode
    @param buffer Pointer to encoded output buffer

    // RRR (Bendes): Next two lines are helpful comments. Above 4 are unhelpful clutter
    //               Use comments sparingly to communicate things that code
    //               can't do clearly alone. So for example I believe an assumption
    //               of this file is that buffer is at least "length" long.
    //               That should be communicated somewhere since it is not done
    //               so by the function definition. Another example: if buffer
    //               and data are not allowed to point at the same piece of memory,
    //               that should probably be communicated (although we typically
    //               just assume this). You can communicate that by a comment, or
    //               you could adjust the function definition to be
    //               cobsEncode(uint8_t* restrict buffer, const void* restrict data, size_t len)
    //               Doing so can also speed up the code. That's only valid in C,
    //               (c++ syntax is like __restrict__ or something like that)
    //               so you can use a similar #define like you did for INLINE
    //               above (something like RESTRICT). Just an example you don't
    //               need to do the restrict thing unless you want to
    @return Encoded buffer length in bytes
    @note Does not output delimiter byte
*/
static INLINE size_t
cobsEncode(const void* data, size_t length, uint8_t* buffer) {
    assert(data && buffer); // RRR (Bendes): Feels unnecessary

    // RRR (Bendes): Code should be self commenting. If your code nees this
    //               many comments, think about how to name your variables
    //               differently so the comments aren't necessary. Comments
    //               that repeat information clearly communicated by code
    //               are a direct violation of DRY.
    //
    uint8_t* encode = buffer;   // Encoded byte pointer
    uint8_t* codep = encode++;  // Output code pointer
    uint8_t code = 1;           // Code value

    for (const uint8_t* byte = (const uint8_t*)data; length--; ++byte) {
        // RRR (Bendes): Audit all of your comments and remove those that aren't
        //               necessary. If you need a comment because your code
        //               doesn't clearly communicate what the comment does, lean
        //               toward making the code clearer - not toward leaving a
        //               comment
        if (*byte)  // Byte not zero, write it
            *encode++ = *byte, ++code;

        // RRR (Bendes): Curlys on same line as if with a space. See other
        //               files in repo for style examples
        if (!*byte || code == 0xff)  // Input is zero or block completed, restart
        {
            // RRR (Bendes): SRP dictates that we have any one line of code
            //               do 1 thing and 1 thing only. Split up your variable
            //               assignemnts on to separate lines
            *codep = code, code = 1, codep = encode;
            // RRR (Bendes): Match style: join lines in an if they fit on one line
            if (!*byte || length)
                ++encode;
        }
    }
    // RRR (Bendes): Final time mentioning comments. Just assume the same
    //               review comment applies to every code comment across
    //               the whole PR
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
// RRR (Bendes): Why is data not a uint8_t* if this whole function assumes it is?
static INLINE size_t
cobsDecode(const uint8_t* buffer, size_t length, void* data) {
    assert(buffer && data); // RRR (Bendes): Feels unnecessary

    const uint8_t* byte = buffer;      // Encoded input byte pointer
    uint8_t* decode = (uint8_t*)data;  // Decoded output byte pointer

    for (uint8_t code = 0xff, block = 0; byte < buffer + length; --block) {
        // RRR (Bendes): If your else has curly braces, make sure your if does too
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
    // RRR (Bendes): Um.......so + 253?
    return (msgSize + 254 - 1) / 254;  // COBS overhead
}

#endif /* _ZCM_TRANS_NONBLOCKING_COBS_H */
