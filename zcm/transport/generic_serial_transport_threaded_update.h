#ifndef _ZCM_TRANS_GENERIC_SERIAL_THREADED_UPDATE_H
#define _ZCM_TRANS_GENERIC_SERIAL_THREADED_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "zcm/transport.h"
#include "zcm/zcm.h"

// This creates a BLOCKING transport that acts equivalently to having a nonblocking
// generic serial transport plus 2 threads, which will each update the tx and rx
// hardware in a loop.
//
// Note, the user is responsible for all internal behaviors of get/put, including
// thread safety (both may be called concurrently), maximum cpu usage (both are
// simply called repeatedly in a loop), and timeout behavior.
zcm_trans_t* zcm_trans_generic_serial_threaded_update_create(
    size_t (*get)(uint8_t* data, size_t nData, uint32_t timeoutMs, void* usr),
    size_t (*put)(const uint8_t* data, size_t nData, uint32_t timeoutMs, void* usr),
    uint32_t timeoutMs, void* put_get_usr, uint64_t (*timestamp_now)(void* usr),
    void* time_usr, size_t MTU, size_t bufSize);

// frees all resources inside of zt and frees zt itself
void zcm_trans_generic_serial_threaded_update_destroy(zcm_trans_t* zt);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANS_GENERIC_SERIAL_THREADED_UPDATE_H */
