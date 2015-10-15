#ifndef _ZCM_H
#define _ZCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef ZCM_EMBEDDED
#include "eventlog.h"
#endif

#define ZCM_CHANNEL_MAXLEN 32

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
    char *data;
    uint32_t data_size;
    int64_t recv_utime;

    zcm_t *zcm; /* TODO: do we need the zcm pointer here? */
};

typedef void (*zcm_msg_handler_t)(const zcm_recv_buf_t *rbuf, const char *channel, void *usr);

typedef struct zcm_sub_t zcm_sub_t;
struct zcm_sub_t
{
    /* Note: this is static to avoid mallocing and allow use of sizeof */
    char channel[ZCM_CHANNEL_MAXLEN+1];
    zcm_msg_handler_t callback;
    void *usr;
};

zcm_t *zcm_create(const char *url);
zcm_t *zcm_create_trans(zcm_trans_t *zt);
void   zcm_destroy(zcm_t *zcm);

/* XXX need to lock down return values for each of these functions (init through unsub) */
/* Returns 1 on success, and 0 on failure */
int  zcm_init(zcm_t *zcm, const char *url);
int  zcm_init_trans(zcm_t *zcm, zcm_trans_t *zt);
void zcm_cleanup(zcm_t *zcm);

int        zcm_publish(zcm_t *zcm, const char *channel, const char *data, uint32_t len);
zcm_sub_t *zcm_subscribe(zcm_t *zcm, const char *channel, zcm_msg_handler_t cb, void *usr);
int        zcm_unsubscribe(zcm_t *zcm, zcm_sub_t *sub);

/* Note: should be used if and only if a block transport is also being used.   */
/*       The start/stop/become functions are recommended, but handle() is also */
/*       provided for backwards compatibility to LCM                           */
void   zcm_become(zcm_t *zcm);
void   zcm_start(zcm_t *zcm);
void   zcm_stop(zcm_t *zcm);
int    zcm_handle(zcm_t *zcm); /* returns 0 nromally, and -1 when an error occurs. */

/* Note: Should be used if and only if a nonblock transport is also being used. Internally, this condition is checked. */
/* Returns 1 if a message was dispatched, and 0 otherwise */
int zcm_handle_nonblock(zcm_t *zcm);

/*
 * Version: M.m.u
 *   M: Major
 *   m: Minor
 *   u: Micro
 */
#define ZCM_MAJOR_VERSION 1
#define ZCM_MINOR_VERSION 0
#define ZCM_MICRO_VERSION 0

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_H */
