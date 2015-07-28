#ifndef _ZCM_H
#define _ZCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct zcm_t zcm_t;

typedef struct zcm_recv_buf_t zcm_recv_buf_t;
struct zcm_recv_buf_t
{
    zcm_t *zcm;

    uint64_t utime;
    size_t len; /* TODO make this datalen for consistency */
    char *data;
};

typedef void zcm_callback_t(const zcm_recv_buf_t *rbuf, const char *channel, void *usr);

zcm_t *zcm_create(const char *url);
void   zcm_destroy(zcm_t *zcm);

// TODO: document return codes
int    zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len);
int    zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr);
// TODO: add an unsubscribe

// NOTE: This call waits for the next message and dispatches to the callbacks
//       The thread that calls this function will be the same as the one that
//       calls the registered callbacks
int    zcm_handle(zcm_t *zcm);

void   zcm_start(zcm_t *zcm);
void   zcm_stop(zcm_t *zcm);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_H */
