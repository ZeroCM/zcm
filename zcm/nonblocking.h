#ifndef _ZCM_NONBLOCKING_H
#define _ZCM_NONBLOCKING_H

#include "zcm/zcm.h"
#include "zcm/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct     zcm_nonblocking zcm_nonblocking_t;
zcm_nonblocking_t *zcm_nonblocking_create(zcm_t *z, zcm_trans_t *trans);
void               zcm_nonblocking_destroy(zcm_nonblocking_t *zcm);

int        zcm_nonblocking_publish(zcm_nonblocking_t *zcm, const char *channel, const char *data,
                                   uint32_t len);
zcm_sub_t *zcm_nonblocking_subscribe(zcm_nonblocking_t *zcm, const char *channel,
                                     zcm_msg_handler_t cb, void *usr);
int        zcm_nonblocking_unsubscribe(zcm_nonblocking_t *zcm, zcm_sub_t *sub);

/* Returns 1 if a message was dispatched, and 0 otherwise */
int zcm_nonblocking_handle_nonblock(zcm_nonblocking_t *zcm);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_NONBLOCKING_H */
