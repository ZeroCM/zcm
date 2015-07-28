#ifndef _ZCM_TRANS_ZMQ_LOCAL_H
#define _ZCM_TRANS_ZMQ_LOCAL_H

#include "zcm/transport.h"
#include "zcm/util/url.h"

#ifdef __cplusplus
extern "C" {
#endif

zcm_trans_t *zcm_trans_ipc_create(zcm_url_t *url);
zcm_trans_t *zcm_trans_inproc_create(zcm_url_t *url);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANS_ZMQ_LOCAL_H */
