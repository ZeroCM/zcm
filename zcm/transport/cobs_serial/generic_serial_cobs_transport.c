#include "zcm/transport/cobs_serial/generic_serial_cobs_transport.h"
#include "zcm/transport.h"
#include "zcm/transport/generic_serial_circ_buff.h"
#include "zcm/transport/generic_serial_fletcher.h"
#include "zcm/util/buffer_utils.h"
#include "zcm/util/cobs.h"
#include "zcm/zcm.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZCM_COBS_SERIAL_TERM_CHAR
#define ZCM_COBS_SERIAL_TERM_CHAR (0x00)
#endif

#define ASSERT(x)

// Framing (size = 8 + chanLen + data_len)
//   chanLen
//   data_len  (4 bytes)
//   *chan
//   *data
//   sum1(*chan, *data)
//   sum2(*chan, *data)
//   0x00 -- termination char
#define FRAME_BYTES 8

typedef struct zcm_trans_cobs_serial_t {
    zcm_trans_t trans;  // This must be first to preserve pointer casting

    circBuffer_t sendBuffer;
    circBuffer_t recvBuffer;
    uint8_t recvChanName[ZCM_CHANNEL_MAXLEN + 1];
    size_t mtu;
    uint8_t* sendMsgData;
    uint8_t* sendMsgDataCobs;
    uint8_t* recvMsgData;
    uint8_t* recvMsgDataCobs;

    size_t (*get)(uint8_t* data, size_t nData, void* usr);
    size_t (*put)(const uint8_t* data, size_t nData, void* usr);
    void* put_get_usr;

    uint64_t (*time)(void* usr);
    void* time_usr;
} zcm_trans_cobs_serial_t;

static zcm_trans_cobs_serial_t* cast(zcm_trans_t* zt);

size_t serial_cobs_get_mtu(zcm_trans_cobs_serial_t *zt)
{ return zt->mtu; }

int
serial_cobs_sendmsg(zcm_trans_cobs_serial_t* zt, zcm_msg_t msg) {
    size_t chanLen = strlen(msg.channel);
    size_t payloadLen = FRAME_BYTES + chanLen + msg.len;

    if (msg.len > zt->mtu || chanLen > ZCM_CHANNEL_MAXLEN) {
        return ZCM_EINVALID;
    }
    if (payloadLen + cobsMaxOverhead(payloadLen) > cb_room(&zt->sendBuffer)) {
        return ZCM_EAGAIN;
    }

    uint8_t* pMsgData = zt->sendMsgData;

    // Copy channel and message length
    *pMsgData++ = chanLen;
    pMsgData = bufferUint32(pMsgData, (uint32_t)msg.len);

    // Copy channel name into msgData
    memcpy(pMsgData, msg.channel, chanLen);
    pMsgData += chanLen;

    // Copy message contents into msgData
    memcpy(pMsgData, msg.buf, msg.len);
    pMsgData += msg.len;

    // Calculate Fletcher-16 checksum and add to msgData
    uint16_t checksum = fletcher16(zt->sendMsgData, payloadLen - 3);
    pMsgData = bufferUint16(pMsgData, checksum);

    // COBS encode the message, subtract 1 for termination char
    size_t bytesEncoded = cobsEncode(zt->sendMsgData, payloadLen - 1, zt->sendMsgDataCobs);
    if (bytesEncoded <= payloadLen - 1) {
        return ZCM_EAGAIN;  // encoding failed for some reason
    }

    // Push entire message into sendBuffer
    for (int i = 0; i < bytesEncoded; ++i) {
        cb_push_back(&zt->sendBuffer, zt->sendMsgDataCobs[i]);
    }
    cb_push_back(&zt->sendBuffer, ZCM_COBS_SERIAL_TERM_CHAR);  // terminator byte

    return ZCM_EOK;
}

int serial_cobs_recvmsg_enable(zcm_trans_cobs_serial_t *zt, const char *channel, bool enable)
{
    // NOTE: not implemented because it is unlikely that a microprocessor is
    //       going to be hearing messages on a USB comms that it doesn't want
    //       to hear
    return ZCM_EOK;
}

int
serial_cobs_recvmsg(zcm_trans_cobs_serial_t* zt, zcm_msg_t* msg, int timeout) {
    uint64_t utime = zt->time(zt->time_usr);
    size_t incomingSize = cb_size(&zt->recvBuffer);
    size_t minMsgSize = FRAME_BYTES + cobsMaxOverhead(FRAME_BYTES);
    if (incomingSize < minMsgSize) {
        return ZCM_EAGAIN;
    }

    // Search for terminator bytes, starting at the back
    int termAddr = -1;
    for (int i = incomingSize - 1; i >= minMsgSize; --i) {
        if (cb_front(&zt->recvBuffer, i) == ZCM_COBS_SERIAL_TERM_CHAR) {
            termAddr = i;  // keep track of terminator closest to front
        }
    }

    // Check if terminator byte was found
    if (termAddr == -1) {
        return ZCM_EAGAIN;
    }

    // Pop CB from front to termAddr
    for (int i = 0; i <= termAddr; ++i) {
        zt->recvMsgDataCobs[i] = cb_front(&zt->recvBuffer, i);
    }
    cb_pop_front(&zt->recvBuffer, termAddr + 1);

    // COBS decode
    size_t decodedBytes = cobsDecode(zt->recvMsgDataCobs, termAddr, zt->recvMsgData);
    if (decodedBytes >= termAddr) {
        return ZCM_EAGAIN;  // decoding failed, probably missing some of message
    }

    // Extract channel and message sizes
    uint8_t chanLen = 0;
    uint8_t* pMsgData = zt->recvMsgData;

    chanLen = *pMsgData++;
    msg->len = pMsgData[0];
    msg->len |= pMsgData[1] << 8;
    msg->len |= pMsgData[2] << 16;
    msg->len |= pMsgData[3] << 24;
    pMsgData += 4;

    // Value rationality checks
    if (chanLen > ZCM_CHANNEL_MAXLEN)
        return ZCM_EAGAIN;

    if (msg->len > zt->mtu)
        return ZCM_EAGAIN;

    if (termAddr != FRAME_BYTES + chanLen + msg->len)
        return ZCM_EAGAIN;

    // Calculate Fletcher-16 checksum and check against received
    uint16_t checksum = 0;
    uint16_t receivedCS = 0;
    checksum = fletcher16(zt->recvMsgData, termAddr - 3);
    receivedCS = zt->recvMsgData[termAddr - 3] | (zt->recvMsgData[termAddr - 2] << 8);
    if (receivedCS != checksum) {
        return ZCM_EINVALID;
    }

    // Copy channel name
    memset(&zt->recvChanName, '\0', ZCM_CHANNEL_MAXLEN);
    memcpy(zt->recvChanName, pMsgData, chanLen);
    pMsgData += chanLen;

    // Copy values into msg
    msg->channel = (char*)zt->recvChanName;
    msg->buf = pMsgData;
    msg->utime = utime;

    return ZCM_EOK;
}

int serial_cobs_update_rx(zcm_trans_t *_zt)
{
    zcm_trans_cobs_serial_t* zt = cast(_zt);
    cb_flush_in(&zt->recvBuffer, zt->get, zt->put_get_usr);
    return ZCM_EOK;
}

int serial_cobs_update_tx(zcm_trans_t *_zt)
{
    zcm_trans_cobs_serial_t* zt = cast(_zt);
    cb_flush_out(&zt->sendBuffer, zt->put, zt->put_get_usr);
    return ZCM_EOK;
}

/********************** STATICS **********************/
static size_t
_serial_get_mtu(zcm_trans_t* zt) {
    return serial_cobs_get_mtu(cast(zt));
}

static int
_serial_sendmsg(zcm_trans_t* zt, zcm_msg_t msg) {
    return serial_cobs_sendmsg(cast(zt), msg);
}

static int
_serial_recvmsg_enable(zcm_trans_t* zt, const char* channel, bool enable) {
    return serial_cobs_recvmsg_enable(cast(zt), channel, enable);
}

static int
_serial_recvmsg(zcm_trans_t* zt, zcm_msg_t* msg, int timeout) {
    return serial_cobs_recvmsg(cast(zt), msg, timeout);
}

static int
_serial_update(zcm_trans_t* zt) {
    int rxRet = serial_cobs_update_rx(zt);
    int txRet = serial_cobs_update_tx(zt);
    return rxRet == ZCM_EOK ? txRet : rxRet;
}

static zcm_trans_methods_t methods = {
    &_serial_get_mtu,
    &_serial_sendmsg,
    &_serial_recvmsg_enable,
    &_serial_recvmsg,
    &_serial_update,
    &zcm_trans_generic_serial_cobs_destroy,
};

static zcm_trans_cobs_serial_t*
cast(zcm_trans_t* zt) {
    assert(zt->vtbl == &methods);
    return (zcm_trans_cobs_serial_t*)zt;
}

zcm_trans_t*
zcm_trans_generic_serial_cobs_create(size_t (*get)(uint8_t* data, size_t nData, void* usr),
                                     size_t (*put)(const uint8_t* data, size_t nData, void* usr),
                                     void* put_get_usr, uint64_t (*timestamp_now)(void* usr),
                                     void* time_usr, size_t MTU, size_t bufSize) {
    if (MTU == 0 || bufSize < FRAME_BYTES + MTU)
        return NULL;
    zcm_trans_cobs_serial_t* zt = malloc(sizeof(zcm_trans_cobs_serial_t));
    if (zt == NULL)
        return NULL;
    zt->mtu = MTU;

    // Bytes needed to construct full message
    size_t maxPayloadSize = FRAME_BYTES + ZCM_CHANNEL_MAXLEN + zt->mtu;
    zt->recvMsgData = malloc(maxPayloadSize * sizeof(uint8_t));
    if (zt->recvMsgData == NULL) {
        free(zt);
        return NULL;
    }
    zt->sendMsgData = malloc(maxPayloadSize * sizeof(uint8_t));
    if (zt->sendMsgData == NULL) {
        free(zt->recvMsgData);
        free(zt);
        return NULL;
    }

    // Bytes needed to construct full COBS-encoded message
    maxPayloadSize += cobsMaxOverhead(maxPayloadSize);
    zt->recvMsgDataCobs = malloc(maxPayloadSize * sizeof(uint8_t));
    if (zt->recvMsgDataCobs == NULL) {
        free(zt->recvMsgData);
        free(zt->sendMsgData);
        free(zt);
        return NULL;
    }
    zt->sendMsgDataCobs = malloc(maxPayloadSize * sizeof(uint8_t));
    if (zt->sendMsgDataCobs == NULL) {
        free(zt->recvMsgData);
        free(zt->sendMsgData);
        free(zt->recvMsgDataCobs);
        free(zt);
        return NULL;
    }

    zt->trans.trans_type = ZCM_NONBLOCKING;
    zt->trans.vtbl = &methods;
    if (!cb_init(&zt->sendBuffer, bufSize)) {
        free(zt->recvMsgData);
        free(zt->sendMsgData);
        free(zt->recvMsgDataCobs);
        free(zt->sendMsgDataCobs);
        free(zt);
        return NULL;
    }
    if (!cb_init(&zt->recvBuffer, bufSize)) {
        cb_deinit(&zt->sendBuffer);
        free(zt->recvMsgData);
        free(zt->sendMsgData);
        free(zt->recvMsgDataCobs);
        free(zt->sendMsgDataCobs);
        free(zt);
        return NULL;
    }

    zt->get = get;
    zt->put = put;
    zt->put_get_usr = put_get_usr;

    zt->time = timestamp_now;
    zt->time_usr = time_usr;

    return (zcm_trans_t*)zt;
}

void
zcm_trans_generic_serial_cobs_destroy(zcm_trans_t* _zt) {
    zcm_trans_cobs_serial_t* zt = cast(_zt);
    cb_deinit(&zt->recvBuffer);
    cb_deinit(&zt->sendBuffer);
    free(zt->recvMsgData);
    free(zt->sendMsgData);
    free(zt->recvMsgDataCobs);
    free(zt->sendMsgDataCobs);
    free(zt);
}
