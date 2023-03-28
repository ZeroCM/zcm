#include "zcm/transport/cobs_serial/generic_serial_cobs_transport.h"

#include "zcm/transport.h"
#include "zcm/transport/generic_serial_circ_buff.h"
#include "zcm/transport/generic_serial_fletcher.h"
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
#define COBS_MAX_FRAME_OVERHEAD 1
static const size_t minMessageSize = FRAME_BYTES + COBS_MAX_FRAME_OVERHEAD;

typedef struct zcm_trans_cobs_serial_t {
    zcm_trans_t trans;  // This must be first to preserve pointer casting

    circBuffer_t sendBuffer;
    circBuffer_t recvBuffer;
    size_t mtu;
    uint8_t recvChanName[ZCM_CHANNEL_MAXLEN + 1];
    uint8_t* sendMsgData;
    uint8_t* recvMsgData;

    size_t (*get)(uint8_t* data, size_t nData, void* usr);
    size_t (*put)(const uint8_t* data, size_t nData, void* usr);
    void* put_get_usr;

    uint64_t (*time)(void* usr);
    void* time_usr;
} zcm_trans_cobs_serial_t;

static zcm_trans_cobs_serial_t* cast(zcm_trans_t* zt);

size_t serial_cobs_get_mtu(zcm_trans_cobs_serial_t* zt) { return zt->mtu; }

/**
 * @brief   COBS encode \p length bytes from \p src to \p dest
 *
 * @param   dest
 * @param   src
 * @param   length
 *
 * @return  Encoded buffer length in bytes
 *
 * @note    Does not encode termination character
 */
static size_t cobs_encode_zcm(circBuffer_t* dest, const uint8_t* src,
                              size_t length)
{
    uint8_t code = 1;
    size_t stuffBytes = 1;
    size_t bytesWritten = 0;

    for (const uint8_t* byte = src; length--; ++byte) {
        if (*byte) { ++code; }

        if (!*byte || code == 0xff) {  // zero or end of block, restart
            cb_push_back(dest, code);
            ++bytesWritten;
            while (--code) {
                cb_push_back(dest, src[bytesWritten - stuffBytes]);
                ++bytesWritten;
            }
            if (*byte) { stuffBytes++; }
            code = 1;
        }
    }
    cb_push_back(dest, code);
    ++bytesWritten;
    while (--code) {
        cb_push_back(dest, src[bytesWritten - stuffBytes]);
        ++bytesWritten;
    }

    return bytesWritten;
}

/**
 * @brief   COBS decode \p length bytes from \p src to \p dest
 *
 * @param   dest
 * @param   src
 * @param   length
 *
 * @return  Decoded buffer length in bytes, excluding delimeter
 *
 * @note    Stops decoding if delimiter byte is found
 * @note    Does not pop any values out of \p src
 */
static size_t cobs_decode_zcm(uint8_t* dest, circBuffer_t* src, size_t length)
{
    bool foundTerm = false;
    uint8_t* decode = dest;
    size_t stuffBytes = 0;
    size_t bytesRead = 0;
    uint8_t byte = cb_front(src, bytesRead++);

    for (uint8_t code = 0xff, block = 0; bytesRead < length; --block) {
        if (block) {
            *decode = byte;
            decode++;
            byte = cb_front(src, bytesRead++);
            continue;
        }

        if (code != 0xff) {
            *decode = 0;
            decode++;
        }
        else {
            stuffBytes++;
        }

        block = code = byte;
        byte = cb_front(src, bytesRead++);

        if (!code) {
            foundTerm = true;
            bytesRead--;
            break;
        }
    }

    return foundTerm ? bytesRead - stuffBytes : 0;
}

int serial_cobs_sendmsg(zcm_trans_cobs_serial_t* zt, zcm_msg_t msg)
{
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
    *pMsgData = (uint8_t)chanLen;
    pMsgData++;

    pMsgData[0] = (uint8_t)(msg.len & 0xFF);
    pMsgData[1] = (uint8_t)((msg.len >> 8) & 0xFF);
    pMsgData[2] = (uint8_t)((msg.len >> 16) & 0xFF);
    pMsgData[3] = (uint8_t)((msg.len >> 24) & 0xFF);
    pMsgData += 4;

    // Copy channel name into msgData
    memcpy(pMsgData, msg.channel, chanLen);
    pMsgData += chanLen;

    // Copy message contents into msgData
    memcpy(pMsgData, msg.buf, msg.len);
    pMsgData += msg.len;

    // Calculate Fletcher-16 checksum and add to msgData
    uint16_t checksum = fletcher16(zt->sendMsgData, payloadLen - 3);
    pMsgData[0] = (uint8_t)(checksum & 0xFF);
    pMsgData[1] = (uint8_t)((checksum >> 8) & 0xFF);

    size_t bytesEncoded =
        cobs_encode_zcm(&zt->sendBuffer, zt->sendMsgData, payloadLen - 1);
    if (bytesEncoded <= payloadLen - 1) {
        return ZCM_EAGAIN;  // encoding failed
    }
    cb_push_back(&zt->sendBuffer, ZCM_COBS_SERIAL_TERM_CHAR);

    return ZCM_EOK;
}

int serial_cobs_recvmsg_enable(zcm_trans_cobs_serial_t* zt, const char* channel,
                               bool enable)
{
    return ZCM_EOK;  // not implemented
}

int serial_cobs_recvmsg(zcm_trans_cobs_serial_t* zt, zcm_msg_t* msg,
                        int timeout)
{
    uint64_t utime = zt->time(zt->time_usr);
    size_t incomingSize = cb_size(&zt->recvBuffer);
    if (incomingSize < minMessageSize) return ZCM_EAGAIN;

    // COBS decode
    size_t bytesDecoded =
        cobs_decode_zcm(zt->recvMsgData, &zt->recvBuffer, incomingSize);
    if (!bytesDecoded) return ZCM_EAGAIN;

    cb_pop_front(&zt->recvBuffer, bytesDecoded + 1);  // +1 for terminator

    // Extract channel and message sizes
    uint8_t* pMsgData = zt->recvMsgData;
    uint8_t chanLen = *pMsgData;
    pMsgData++;

    msg->len = pMsgData[0];
    msg->len |= pMsgData[1] << 8;
    msg->len |= pMsgData[2] << 16;
    msg->len |= pMsgData[3] << 24;
    pMsgData += 4;

    // Value rationality checks
    if (chanLen > ZCM_CHANNEL_MAXLEN || msg->len > zt->mtu) return ZCM_EINVALID;

    // Calculate Fletcher-16 checksum and check against received
    uint16_t checksum = 0;
    uint16_t receivedCS = 0;
    checksum = fletcher16(zt->recvMsgData, bytesDecoded - 3);
    receivedCS = zt->recvMsgData[bytesDecoded - 3] |
                 (zt->recvMsgData[bytesDecoded - 2] << 8);

    if (receivedCS != checksum) return ZCM_EINVALID;

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

int serial_cobs_update_rx(zcm_trans_t* _zt)
{
    zcm_trans_cobs_serial_t* zt = cast(_zt);
    cb_flush_in(&zt->recvBuffer, zt->get, zt->put_get_usr);
    return ZCM_EOK;
}

int serial_cobs_update_tx(zcm_trans_t* _zt)
{
    zcm_trans_cobs_serial_t* zt = cast(_zt);
    cb_flush_out(&zt->sendBuffer, zt->put, zt->put_get_usr);
    return ZCM_EOK;
}

/********************** STATICS **********************/
static size_t _serial_get_mtu(zcm_trans_t* zt)
{
    return serial_cobs_get_mtu(cast(zt));
}

static int _serial_sendmsg(zcm_trans_t* zt, zcm_msg_t msg)
{
    return serial_cobs_sendmsg(cast(zt), msg);
}

static int _serial_recvmsg_enable(zcm_trans_t* zt, const char* channel,
                                  bool enable)
{
    return serial_cobs_recvmsg_enable(cast(zt), channel, enable);
}

static int _serial_recvmsg(zcm_trans_t* zt, zcm_msg_t* msg, int timeout)
{
    return serial_cobs_recvmsg(cast(zt), msg, timeout);
}

static int _serial_update(zcm_trans_t* zt)
{
    int rxRet = serial_cobs_update_rx(zt);
    int txRet = serial_cobs_update_tx(zt);
    return rxRet == ZCM_EOK ? txRet : rxRet;
}

static zcm_trans_methods_t methods = {
    &_serial_get_mtu, &_serial_sendmsg, &_serial_recvmsg_enable,
    &_serial_recvmsg, &_serial_update,  &zcm_trans_generic_serial_cobs_destroy,
};

static zcm_trans_cobs_serial_t* cast(zcm_trans_t* zt)
{
    assert(zt->vtbl == &methods);
    return (zcm_trans_cobs_serial_t*)zt;
}

zcm_trans_t* zcm_trans_generic_serial_cobs_create(
    size_t (*get)(uint8_t* data, size_t nData, void* usr),
    size_t (*put)(const uint8_t* data, size_t nData, void* usr),
    void* put_get_usr, uint64_t (*timestamp_now)(void* usr), void* time_usr,
    size_t MTU, size_t bufSize)
{
    if (MTU == 0 || bufSize < FRAME_BYTES + MTU) return NULL;
    zcm_trans_cobs_serial_t* zt = malloc(sizeof(zcm_trans_cobs_serial_t));
    if (zt == NULL) return NULL;
    zt->mtu = MTU;

    zt->sendMsgData = NULL;
    zt->recvMsgData = NULL;
    zt->recvBuffer.data = NULL;
    zt->sendBuffer.data = NULL;

    size_t maxPayloadSize = FRAME_BYTES + ZCM_CHANNEL_MAXLEN + zt->mtu;
    zt->recvMsgData = malloc(maxPayloadSize * sizeof(uint8_t));
    if (zt->recvMsgData == NULL) goto fail;

    zt->sendMsgData = malloc(maxPayloadSize * sizeof(uint8_t));
    if (zt->sendMsgData == NULL) goto fail;

    if (!cb_init(&zt->recvBuffer, bufSize)) goto fail;
    if (!cb_init(&zt->sendBuffer, bufSize)) goto fail;

    zt->trans.trans_type = ZCM_NONBLOCKING;
    zt->trans.vtbl = &methods;

    zt->get = get;
    zt->put = put;
    zt->put_get_usr = put_get_usr;

    zt->time = timestamp_now;
    zt->time_usr = time_usr;

    return (zcm_trans_t*)zt;

fail:
    if (zt->recvBuffer.data != NULL) cb_deinit(&zt->recvBuffer);
    if (zt->sendBuffer.data != NULL) cb_deinit(&zt->sendBuffer);
    if (zt->recvMsgData != NULL) free(zt->recvMsgData);
    if (zt->sendMsgData != NULL) free(zt->sendMsgData);

    free(zt);
    return NULL;
}

void zcm_trans_generic_serial_cobs_destroy(zcm_trans_t* _zt)
{
    zcm_trans_cobs_serial_t* zt = cast(_zt);
    cb_deinit(&zt->recvBuffer);
    cb_deinit(&zt->sendBuffer);
    free(zt->recvMsgData);
    free(zt->sendMsgData);
    free(zt);
}
