#ifndef _ZCM_TRANS_ZMQ_LOCAL_H
#define _ZCM_TRANS_ZMQ_LOCAL_H

#include "zcm/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

zcm_trans_t *zcm_trans_ipc_create(void);
zcm_trans_t *zcm_trans_inproc_create(void);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANS_ZMQ_LOCAL_H */
