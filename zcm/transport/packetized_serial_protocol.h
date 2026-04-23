#ifndef _ZCM_TRANS_PACKETIZED_SERIAL_PROTOCOL_H
#define _ZCM_TRANS_PACKETIZED_SERIAL_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "generic_serial_fletcher.h"

#define PACKETIZED_HEADER_BYTES (4)
#define PACKETIZED_DATA_OVERHEAD_BYTES (2)
#define PACKETIZED_METADATA_BODY_BYTES (9)
#define PACKETIZED_RETRANS_TIMEOUT_US (200000)
#define PACKETIZED_MAX_PACKET_DATA_SIZE (253)
#define PACKETIZED_MAX_PACKETS ((size_t)UINT16_MAX)
#define PACKETIZED_DEFAULT_MAX_MESSAGE_SIZE (1024)

typedef enum packetized_msg_type_t
{
    PACKETIZED_MSG_METADATA    = 1,
    PACKETIZED_MSG_DATA        = 2,
    PACKETIZED_MSG_RETRANS_REQ = 3,
} packetized_msg_type_t;

static inline size_t packetized_message_packet_count(size_t message_size,
                                                     uint8_t packet_data_size)
{
    if (packet_data_size == 0) return 0;
    if (message_size == 0) return 1;
    return (message_size + packet_data_size - 1u) / packet_data_size;
}

#endif /* _ZCM_TRANS_PACKETIZED_SERIAL_PROTOCOL_H */
