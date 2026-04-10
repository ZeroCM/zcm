#ifndef _ZCM_TRANS_PACKETIZED_SERIAL_H
#define _ZCM_TRANS_PACKETIZED_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "zcm/transport.h"
#include "zcm/zcm.h"
#include "packetized_serial_protocol.h"

/*
 * Channel name convention
 * -----------------------
 * The packetized transport uses the leading '$' character as a signal that a
 * message should be fragmented and reassembled using the packetization protocol.
 * Any channel whose name begins with '$' is treated as a packetized channel;
 * the '$' is stripped from the channel name on delivery to the receiver.
 * Channels without a leading '$' are passed through to the underlying generic
 * serial transport unchanged and are not packetized.
 */

zcm_trans_t* zcm_trans_packetized_serial_create(
    size_t (*get)(uint8_t* data, size_t nData, void* usr),
    size_t (*put)(const uint8_t* data, size_t nData, void* usr), void* put_get_usr,
    uint64_t (*timestamp_now)(void* usr), void* time_usr, size_t MTU, size_t bufSize,
    uint8_t packet_data_size, size_t max_message_size);

void zcm_trans_packetized_serial_destroy(zcm_trans_t* zt);

int packetized_serial_update_rx(zcm_trans_t* zt);
int packetized_serial_update_tx(zcm_trans_t* zt);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANS_PACKETIZED_SERIAL_H */
