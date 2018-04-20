#include "zcm/zcm.h"
#include "zcm/transport.h"
#include "generic_serial_transport.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZCM_GENERIC_SERIAL_MTU
#define ZCM_GENERIC_SERIAL_MTU 128
#endif

#ifndef ZCM_GENERIC_SERIAL_BUFFER_SIZE
#define ZCM_GENERIC_SERIAL_BUFFER_SIZE 5*ZCM_GENERIC_SERIAL_MTU+5*ZCM_CHANNEL_MAXLEN
#endif

#ifndef ZCM_GENERIC_SERIAL_ESCAPE_CHAR
#define ZCM_GENERIC_SERIAL_ESCAPE_CHAR (0xcc)
#endif

/* Framing (size = 8 + chan_len + data_len)
 *   0xCC
 *   0x00
 *   chan_len
 *   data_len  (4 bytes)
 *   *chan
 *   *data
 *   sum1(*chan, *data)
 *   sum2(*chan, *data)
 */
#define FRAME_BYTES 9

/* Note: there is little to no error checking in this, misuse will cause problems */
typedef struct circBuffer_t circBuffer_t;
struct circBuffer_t
{
    uint8_t data[ZCM_GENERIC_SERIAL_BUFFER_SIZE];
    int front;
    int back;
};

void cb_init(circBuffer_t* cb)
{
    cb->front = 0;
    cb->back  = 0;
}

int cb_size(circBuffer_t* cb)
{
    if (cb->back >= cb->front) return cb->back - cb->front;
    else                       return ZCM_GENERIC_SERIAL_BUFFER_SIZE - (cb->front - cb->back);
}

int cb_room(circBuffer_t* cb)
{
    return ZCM_GENERIC_SERIAL_BUFFER_SIZE - 1 - cb_size(cb);
}

void cb_push(circBuffer_t* cb, uint8_t d)
{
    cb->data[cb->back++] = d;
    if (cb->back >= ZCM_GENERIC_SERIAL_BUFFER_SIZE) cb->back = 0;
}

uint8_t cb_top(circBuffer_t* cb, uint32_t offset)
{
    int val = cb->front + offset;
    while (val >= ZCM_GENERIC_SERIAL_BUFFER_SIZE) val -= ZCM_GENERIC_SERIAL_BUFFER_SIZE;
    return cb->data[val];
}

void cb_pop(circBuffer_t* cb, uint32_t num)
{
    cb->front += num;
    while (cb->front >= ZCM_GENERIC_SERIAL_BUFFER_SIZE)
        cb->front -= ZCM_GENERIC_SERIAL_BUFFER_SIZE;
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
uint32_t cb_flush_out(circBuffer_t* cb,
                      uint32_t (*write)(const uint8_t* data, uint32_t num, void* usr),
                      void* usr)
{
	uint32_t written = 0;
	uint32_t n;

    uint32_t contiguous = MIN(ZCM_GENERIC_SERIAL_BUFFER_SIZE - cb->front, cb_size(cb));
    uint32_t wrapped    = cb_size(cb) - contiguous;

    n = write(cb->data + cb->front, contiguous, usr);
    written += n;
    cb_pop(cb, n);

    /* If we failed to write everything we tried to write, or if there's nothing
       left to write, return. */
    if (written != contiguous || wrapped == 0) return written;

    n = write(cb->data, wrapped, usr);
    written += n;
    cb_pop(cb, n);
    return written;
}

uint32_t cb_flush_in(circBuffer_t* cb, uint32_t bytes,
                     uint32_t (*read)(uint8_t* data, uint32_t num, void* usr),
                     void* usr)
{
	uint32_t bytesRead = 0;
	uint32_t n;

    /* Next, find out how much room is left between back and end of buffer or back and front
       of buffer. Because we already know there's room for whatever we're about to place,
       if back < front, we can just read in every byte starting at "back". */
    if (cb->back < cb->front) {
    	bytesRead += read(cb->data + cb->back, bytes, usr);
        cb->back += bytesRead;
        return bytesRead;
    }

    /* Otherwise, we need to be a bit more careful about overflowing the back of the buffer. */
    uint32_t contiguous = MIN(ZCM_GENERIC_SERIAL_BUFFER_SIZE - cb->back, bytes);
    uint32_t wrapped    = bytes - contiguous;

    n = read(cb->data + cb->back, contiguous, usr);
    bytesRead += n;
    cb->back += n;
    if (n != contiguous) return bytesRead; /* back could NOT have hit BUFFER_SIZE in this case */

    /* may need to wrap back here (if bytes >= BUFFER_SIZE - cb->back) but not otherwise */
    if (cb->back >= ZCM_GENERIC_SERIAL_BUFFER_SIZE) cb->back = 0;
    if (wrapped == 0) return bytesRead;

    n = read(cb->data, wrapped, usr);
    bytesRead += n;
    cb->back += n;
    return bytesRead;
}

#undef MIN

static uint16_t fletcherUpdate(uint8_t b, uint16_t prevSum)
{
    uint16_t sumHigh = (prevSum >> 8) & 0xff;
    uint16_t sumLow  =  prevSum       & 0xff;
    sumHigh += sumLow += b;

    sumLow  = (sumLow  & 0xff) + (sumLow  >> 8);
    sumHigh = (sumHigh & 0xff) + (sumHigh >> 8);

    /* Note: double reduction to ensure no overflow after first */
    sumLow  = (sumLow  & 0xff) + (sumLow  >> 8);
    sumHigh = (sumHigh & 0xff) + (sumHigh >> 8);

    return (sumHigh << 8) | sumLow;
}

typedef struct zcm_trans_generic_serial_t zcm_trans_generic_serial_t;
struct zcm_trans_generic_serial_t
{
    zcm_trans_t trans; /* This must be first to preserve pointer casting */

    circBuffer_t sendBuffer;
    circBuffer_t recvBuffer;
    char         recvChanName[ZCM_CHANNEL_MAXLEN+1];
    uint8_t      recvMsgData[ZCM_GENERIC_SERIAL_MTU];

    uint32_t (*get)(uint8_t* data, uint32_t nData, void* usr);
    uint32_t (*put)(const uint8_t* data, uint32_t nData, void* usr);
    void* put_get_usr;

    uint64_t (*time)(void* usr);
    void* time_usr;
};

size_t serial_get_mtu(zcm_trans_generic_serial_t *zt)
{
    return ZCM_GENERIC_SERIAL_MTU;
}

int serial_sendmsg(zcm_trans_generic_serial_t *zt, zcm_msg_t msg)
{
    size_t chan_len = strlen(msg.channel);
    size_t nPushed = 0;

    if (chan_len > ZCM_CHANNEL_MAXLEN)                               return ZCM_EINVALID;
    if (msg.len > ZCM_GENERIC_SERIAL_MTU)                            return ZCM_EINVALID;
    if (FRAME_BYTES + chan_len + msg.len > cb_room(&zt->sendBuffer)) return ZCM_EAGAIN;

    cb_push(&zt->sendBuffer, ZCM_GENERIC_SERIAL_ESCAPE_CHAR); ++nPushed;
    cb_push(&zt->sendBuffer, 0x00);                           ++nPushed;
    cb_push(&zt->sendBuffer, chan_len);                       ++nPushed;

    uint32_t len = (uint32_t)msg.len;
    cb_push(&zt->sendBuffer, (len>>24)&0xff); ++nPushed;
    cb_push(&zt->sendBuffer, (len>>16)&0xff); ++nPushed;
    cb_push(&zt->sendBuffer, (len>> 8)&0xff); ++nPushed;
    cb_push(&zt->sendBuffer, (len>> 0)&0xff); ++nPushed;

    uint16_t sum = 0xffff;
    int i;
    for (i = 0; i < chan_len; ++i) {
        uint8_t c = (uint8_t) msg.channel[i];

        cb_push(&zt->sendBuffer, c); ++nPushed;

        if (c == ZCM_GENERIC_SERIAL_ESCAPE_CHAR) {
        	/* the escape character doesn't count, so we have chan_len - i characters
        	   remaining in channel + the msg + the checksum. */
            if (cb_room(&zt->sendBuffer) > chan_len - i + msg.len + 1) {
                cb_push(&zt->sendBuffer, c); ++nPushed;
            } else {
                cb_pop(&zt->sendBuffer, nPushed);
                return ZCM_EAGAIN;
            }
        }

        sum = fletcherUpdate(c, sum);
    }

    for (i = 0; i < msg.len; ++i) {
        uint8_t c = (uint8_t) msg.buf[i];

        cb_push(&zt->sendBuffer, c); ++nPushed;

        if (c == ZCM_GENERIC_SERIAL_ESCAPE_CHAR) {
        	/* the escape character doesn't count, so we have msg.len - i characters
        	   remaining in the msg + the checksum. */
            if (cb_room(&zt->sendBuffer) > msg.len - i + 1) {
                cb_push(&zt->sendBuffer, c); ++nPushed;
            } else {
                cb_pop(&zt->sendBuffer, nPushed);
                return ZCM_EAGAIN;
            }
        }

        sum = fletcherUpdate(c, sum);
    }

    cb_push(&zt->sendBuffer, (sum >> 8) & 0xff); ++nPushed;
    cb_push(&zt->sendBuffer,  sum       & 0xff); ++nPushed;

    return ZCM_EOK;
}

int serial_recvmsg_enable(zcm_trans_generic_serial_t *zt, const char *channel, bool enable)
{
    /* NOTE: not implemented because it is unlikely that a microprocessor is
             going to be hearing messages on a USB comms that it doesn't want
             to hear */
    return ZCM_EOK;
}

int serial_recvmsg(zcm_trans_generic_serial_t *zt, zcm_msg_t *msg, int timeout)
{
    uint64_t utime = zt->time(zt->time_usr);
    int incomingSize = cb_size(&zt->recvBuffer);
    if (incomingSize < FRAME_BYTES)
        return ZCM_EAGAIN;

    uint32_t consumed = 0;

    /* Sync */
    if (cb_top(&zt->recvBuffer, consumed++) != ZCM_GENERIC_SERIAL_ESCAPE_CHAR) goto fail;
    if (cb_top(&zt->recvBuffer, consumed++) != 0x00       )                    goto fail;

    /* Msg sizes */
    uint8_t chan_len = cb_top(&zt->recvBuffer, consumed++);
    msg->len  = cb_top(&zt->recvBuffer, consumed++) << 24;
    msg->len |= cb_top(&zt->recvBuffer, consumed++) << 16;
    msg->len |= cb_top(&zt->recvBuffer, consumed++) << 8;
    msg->len |= cb_top(&zt->recvBuffer, consumed++);

    if (chan_len > ZCM_CHANNEL_MAXLEN)     goto fail;
    if (msg->len > ZCM_GENERIC_SERIAL_MTU) goto fail;

    if (incomingSize < FRAME_BYTES + chan_len + msg->len) return ZCM_EAGAIN;

    memset(&zt->recvChanName, '0', ZCM_CHANNEL_MAXLEN);

    uint16_t sum = 0xffff;
    int i;
    for (i = 0; i < chan_len; ++i) {

        uint8_t c = cb_top(&zt->recvBuffer, consumed++);

        if (c == ZCM_GENERIC_SERIAL_ESCAPE_CHAR) {
        	/* the escape character doesn't count, so we have chan_len - i characters
        	   remaining in channel + the msg + the checksum. */
            if (consumed + chan_len - i + msg->len + 1 > incomingSize) return ZCM_EAGAIN;

            c = cb_top(&zt->recvBuffer, consumed++);

            if (c != ZCM_GENERIC_SERIAL_ESCAPE_CHAR) {
                consumed-=2;
                goto fail;
            }
        }

        zt->recvChanName[i] = c;
        sum = fletcherUpdate(c, sum);
    }

    zt->recvChanName[chan_len] = '\0';

    for (i = 0; i < msg->len; ++i) {
        if (consumed > incomingSize) return ZCM_EAGAIN;

        uint8_t c = cb_top(&zt->recvBuffer, consumed++);

        if (c == ZCM_GENERIC_SERIAL_ESCAPE_CHAR) {
        	/* the escape character doesn't count, so we have msg.len - i characters
        	   remaining in the msg + the checksum. */
            if (consumed + msg->len - i + 1 > incomingSize) return ZCM_EAGAIN;

            c = cb_top(&zt->recvBuffer, consumed++);

            if (c != ZCM_GENERIC_SERIAL_ESCAPE_CHAR) {
                consumed-=2;
                goto fail;
            }
        }

        zt->recvMsgData[i] = c;
        sum = fletcherUpdate(c, sum);
    }

    uint8_t expectedHigh = cb_top(&zt->recvBuffer, consumed++);
    uint8_t expectedLow  = cb_top(&zt->recvBuffer, consumed++);

    uint16_t received = (expectedHigh << 8) | expectedLow;
    if (received == sum) {
        msg->channel = zt->recvChanName;
        msg->buf     = zt->recvMsgData;
        msg->utime   = utime;
        cb_pop(&zt->recvBuffer, consumed);
        return ZCM_EOK;
    }

  fail:
    cb_pop(&zt->recvBuffer, consumed);
    /* Note: because this is a nonblocking transport, timeout is ignored, so we don't need
             to subtract the time used here */
    return serial_recvmsg(zt, msg, timeout);
}

int serial_update(zcm_trans_generic_serial_t *zt)
{
    cb_flush_in(&zt->recvBuffer, cb_room(&zt->recvBuffer), zt->get, zt->put_get_usr);
    cb_flush_out(&zt->sendBuffer, zt->put, zt->put_get_usr);

    return ZCM_EOK;
}

/********************** STATICS **********************/
static zcm_trans_generic_serial_t *cast(zcm_trans_t *zt);

static size_t _serial_get_mtu(zcm_trans_t *zt)
{ return serial_get_mtu(cast(zt)); }

static int _serial_sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
{ return serial_sendmsg(cast(zt), msg); }

static int _serial_recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)
{ return serial_recvmsg_enable(cast(zt), channel, enable); }

static int _serial_recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
{ return serial_recvmsg(cast(zt), msg, timeout); }

static int _serial_update(zcm_trans_t *zt)
{ return serial_update(cast(zt)); }

static void _serial_destroy(zcm_trans_t *zt)
{ free(cast(zt)); }

static zcm_trans_methods_t methods = {
    &_serial_get_mtu,
    &_serial_sendmsg,
    &_serial_recvmsg_enable,
    &_serial_recvmsg,
    &_serial_update,
    &_serial_destroy,
};

static zcm_trans_generic_serial_t *cast(zcm_trans_t *zt)
{
    assert(zt->vtbl == &methods);
    return (zcm_trans_generic_serial_t*)zt;
}

zcm_trans_t *zcm_trans_generic_serial_create(
        uint32_t (*get)(uint8_t* data, uint32_t nData, void* usr),
        uint32_t (*put)(const uint8_t* data, uint32_t nData, void* usr),
        void* put_get_usr,
        uint64_t (*timestamp_now)(void* usr),
        void* time_usr)
{
    zcm_trans_generic_serial_t *zt = malloc(sizeof(zcm_trans_generic_serial_t));
    if (zt == NULL) return NULL;

    zt->trans.trans_type = ZCM_NONBLOCKING;
    zt->trans.vtbl = &methods;
    cb_init(&zt->sendBuffer);
    cb_init(&zt->recvBuffer);

    zt->get  = get;
    zt->put  = put;
    zt->put_get_usr = put_get_usr;

    zt->time = timestamp_now;
    zt->time_usr = time_usr;

    return (zcm_trans_t*) zt;
}
