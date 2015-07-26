#ifndef _ZCM_TRANS_SERIAL_H
#define _ZCM_TRANS_SERIAL_H

#include "transport.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

zcm_trans_t *zcm_trans_serial_create(zcm_url_t *url);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANS_SERIAL_H */
