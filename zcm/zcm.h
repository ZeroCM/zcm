#ifndef _ZCM_H
#define _ZCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct zcm_trans_t zcm_trans_t;

typedef struct zcm_t zcm_t;
enum zcm_type { ZCM_BLOCKING, ZCM_NONBLOCKING };
struct zcm_t
{
    enum zcm_type type;
    void *impl;
};

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
zcm_t *zcm_create_trans(zcm_trans_t *zt);
void   zcm_destroy(zcm_t *zcm);

// Returns 1 on success, and 0 on failure
int    zcm_init(zcm_t *zcm, const char *url);
int    zcm_init_trans(zcm_t *zcm, zcm_trans_t *zt);
void   zcm_cleanup(zcm_t *zcm);

int    zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len);
int    zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr);
// TODO: add an unsubscribe

void   zcm_become(zcm_t *zcm);
//void   zcm_start(zcm_t *zcm);
//void   zcm_stop(zcm_t *zcm);

// Note: Should be used if and only if a nonblock transport is also being used. Internally, this condition is checked.
// Returns 1 if a message was dispatched, and 0 otherwise
int    zcm_handle_nonblock(zcm_t *zcm);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_H */
