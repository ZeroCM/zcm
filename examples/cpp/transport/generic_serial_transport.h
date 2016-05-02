#ifndef _ZCM_TRANS_TIVAUSB_H
#define _ZCM_TRANS_TIVAUSB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zcm/zcm.h"
#include "zcm/transport.h"

// RRR: have we thought about where this transport belongs? Is it really an example, should
//      it be in the main zcm repo, or should it be somewhere else entirely?
zcm_trans_t *zcm_trans_generic_serial_create(
        uint32_t (*get)(uint8_t* data, uint32_t nData),
        uint32_t (*put)(const uint8_t* data, uint32_t nData));

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANS_TIVAUSB_H */
