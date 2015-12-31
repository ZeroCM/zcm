#ifndef _ZCM_BLOCKING_H
#define _ZCM_BLOCKING_H

#include "zcm/zcm.h"
#include "zcm/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct  zcm_blocking zcm_blocking_t;
zcm_blocking_t *zcm_blocking_create(zcm_t *z, zcm_trans_t *trans);
void            zcm_blocking_destroy(zcm_blocking_t *zcm);

int        zcm_blocking_publish(zcm_blocking_t *zcm, const char *channel, const char *data,
                                uint32_t len);
zcm_sub_t *zcm_blocking_subscribe(zcm_blocking_t *zcm, const char *channel, zcm_msg_handler_t cb,
                                  void *usr);
int        zcm_blocking_unsubscribe(zcm_blocking_t *zcm, zcm_sub_t *sub);

void zcm_blocking_flush(zcm_blocking_t *zcm);

void   zcm_blocking_run(zcm_blocking_t *zcm);
void   zcm_blocking_start(zcm_blocking_t *zcm);
void   zcm_blocking_stop(zcm_blocking_t *zcm);
int    zcm_blocking_handle(zcm_blocking_t *zcm);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_BLOCKING_H */
