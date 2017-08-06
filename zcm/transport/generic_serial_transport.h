#ifndef _ZCM_TRANS_NONBLOCKING_SERIAL_H
#define _ZCM_TRANS_NONBLOCKING_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zcm/zcm.h"
#include "zcm/transport.h"

zcm_trans_t *zcm_trans_generic_serial_create(
        uint32_t (*get)(uint8_t* data, uint32_t nData, void* usr),
        uint32_t (*put)(const uint8_t* data, uint32_t nData, void* usr),
        void* put_get_usr,
        uint64_t (*timestamp_now)(void* usr),
        void* time_usr);

// TODO: Make a destroy

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANS_NONBLOCKING_SERIAL_H */
