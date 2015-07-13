#ifndef _ZCM_H
#define _ZCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef struct zcm zcm_t;

typedef struct zcm_recv_buf_t zcm_recv_buf_t;
struct zcm_recv_buf_t
{
    char *data;
    size_t len;
    uint64_t utime;
    zcm_t *zcm;
};

typedef void zcm_callback_t(const zcm_recv_buf_t *rbuf, const char *channel, void *usr);

zcm_t *zcm_create(void);
void   zcm_destroy(zcm_t *zcm);

int    zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len);
int    zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_H */
