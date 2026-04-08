#ifndef _ZCM_TRANS_PACKETIZED_SERIAL_H
#define _ZCM_TRANS_PACKETIZED_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "zcm/transport.h"
#include "zcm/zcm.h"

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

#ifdef __cplusplus
#include <cstdlib>
#include <string>

// Parse a "pkt_size" URL option value into a packet_data_size byte.
// Returns true and sets out on success; returns false on invalid input.
// Valid range is [1, 253].
static inline bool parsePacketDataSize(const std::string* opt, uint8_t& out)
{
    if (!opt) { out = 0; return true; }
    char* endptr;
    unsigned long parsed = strtoul(opt->c_str(), &endptr, 10);
    if (*endptr != '\0' || parsed == 0 || parsed > 253) return false;
    out = (uint8_t)parsed;
    return true;
}

// Parse a "pkt_buf_size" URL option value into a max_message_size.
// Returns true and sets out on success; returns false on invalid input.
// A null opt leaves out unchanged (caller should pre-set the default).
static inline bool parsePacketBufSize(const std::string* opt, size_t& out)
{
    if (!opt) return true;
    char* endptr;
    unsigned long parsed = strtoul(opt->c_str(), &endptr, 10);
    if (*endptr != '\0' || parsed == 0) return false;
    out = (size_t)parsed;
    return true;
}
#endif /* __cplusplus */

#endif /* _ZCM_TRANS_PACKETIZED_SERIAL_H */
