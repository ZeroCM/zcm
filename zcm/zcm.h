#ifndef _ZCM_H
#define _ZCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

typedef struct zcm zcm_t;
typedef int zcm_callback_t(zcm_t *zcm, const char *channel, char *data, size_t len, void *usr);

zcm_t *zcm_create(void);
void   zcm_destroy(zcm_t *zcm);

int    zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len);
int    zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_H */
