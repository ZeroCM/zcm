#ifndef _ZCM_TRANS_PACKETIZED_SERIAL_PROTOCOL_H
#define _ZCM_TRANS_PACKETIZED_SERIAL_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define PACKETIZED_HEADER_BYTES (4)
#define PACKETIZED_DATA_OVERHEAD_BYTES (2)
#define PACKETIZED_METADATA_BODY_BYTES (11)
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

static inline uint16_t packetized_read_u16_be(const uint8_t* p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static inline uint32_t packetized_read_u32_be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static inline void packetized_write_u16_be(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xff);
    p[1] = (uint8_t)(v & 0xff);
}

static inline void packetized_write_u32_be(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xff);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >> 8) & 0xff);
    p[3] = (uint8_t)(v & 0xff);
}

static inline uint32_t packetized_crc32_update_byte(uint32_t crc, uint8_t b)
{
    int i;
    crc ^= b;
    for (i = 0; i < 8; ++i) {
        uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
        crc           = (crc >> 1) ^ (0xedb88320u & mask);
    }
    return crc;
}

static inline uint32_t packetized_crc32_compute(const uint8_t* data, size_t len)
{
    size_t   i;
    uint32_t crc = 0xffffffffu;
    for (i = 0; i < len; ++i) crc = packetized_crc32_update_byte(crc, data[i]);
    return ~crc;
}

static inline size_t packetized_message_packet_count(size_t message_size,
                                                     uint8_t packet_data_size)
{
    if (packet_data_size == 0) return 0;
    if (message_size == 0) return 1;
    return (message_size + packet_data_size - 1u) / packet_data_size;
}

#endif /* _ZCM_TRANS_PACKETIZED_SERIAL_PROTOCOL_H */
