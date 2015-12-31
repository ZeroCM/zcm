#ifndef _ZCM_H
#define _ZCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef ZCM_EMBEDDED
#include "eventlog.h"
#endif

/* Important hardcoded values */
#define ZCM_CHANNEL_MAXLEN 32
enum zcm_type {
    ZCM_BLOCKING,
    ZCM_NONBLOCKING
};

/* Return codes */
enum zcm_return_codes {
    ZCM_EOK       = 0,
    ZCM_EINVALID  = 1,
    ZCM_EAGAIN    = 2,
    ZCM_ECONNECT  = 3,
    ZCM_EINTR     = 4,
    ZCM__RESERVED_COUNT,
    ZCM_EUNKNOWN  = 255,
};

/* Forward typedef'd structs */
typedef struct zcm_trans_t zcm_trans_t;
typedef struct zcm_t zcm_t;
typedef struct zcm_recv_buf_t zcm_recv_buf_t;
typedef struct zcm_sub_t zcm_sub_t;

/* Generic message handler function type */
typedef void (*zcm_msg_handler_t)(const zcm_recv_buf_t *rbuf,
                                  const char *channel, void *usr);

/* Primary ZCM object that handles all top-level interactions including
   delegation between blocking and non-blocking interfaces */
struct zcm_t
{
    enum zcm_type type;
    void *impl;
    int err; /* the last error code */
};

/* ZCM Receive buffer for one message */
struct zcm_recv_buf_t
{
    char *data;           /* NOTE: do not free, the library manages this memory */
    uint32_t data_size;
    int64_t recv_utime;   /* NOTE: only set properly in blocking API */
    zcm_t *zcm;
};

/* Standard create/destroy functions. These will malloc() and free() the zcm_t object.
   Sets zcm errno on failure */
zcm_t *zcm_create(const char *url);
zcm_t *zcm_create_trans(zcm_trans_t *zt);
void   zcm_destroy(zcm_t *zcm);

/* Initialize a zcm object allocated by caller
   Returns 0 on success, and -1 on failure
   Sets zcm errno on failure */
int  zcm_init(zcm_t *zcm, const char *url);

/* Initialize a zcm instance allocated by caller using a transport provided by caller
   Returns 0 on success, and -1 on failure
   Sets zcm errno on failure */
int  zcm_init_trans(zcm_t *zcm, zcm_trans_t *zt);

/* Cleanup a zcm object allocated by caller */
void zcm_cleanup(zcm_t *zcm);

/* Return the last error: a valid from enum zcm_return_codes */
int zcm_errno(zcm_t *zcm);

/* Return the last error in string format */
const char *zcm_strerror(zcm_t *zcm);

/* Subscribe to zcm messages
   Returns a subscription object on success, and NULL on failure
   Does NOT set zcm errno on failure */
zcm_sub_t *zcm_subscribe(zcm_t *zcm, const char *channel, zcm_msg_handler_t cb, void *usr);

/* Unsubscribe to zcm messages, freeing the subscription object
   Returns 0 on success, and -1 on failure
   Does NOT set zcm errno on failure */
int zcm_unsubscribe(zcm_t *zcm, zcm_sub_t *sub);

/* Publish a zcm message buffer. Note: the message may not be completely
   sent after this call has returned. To block until the messages are transmitted,
   call the zcm_flush() method.
   Returns 0 on success, and -1 on failure
   Sets zcm errno on failure */
int  zcm_publish(zcm_t *zcm, const char *channel, const void *data, uint32_t len);

/* Blocking until all published messages have been sent. This should not be
   called concurrently with zcm_publish(). This function may cause all calls to
   zcm_publish() to block. This function is only useful in ZCM's blocking
   transport mode */
void zcm_flush(zcm_t *zcm);

/* Blocking Mode Only: Functions for controlling the message dispatch loop */
void   zcm_run(zcm_t *zcm);
void   zcm_start(zcm_t *zcm);
void   zcm_stop(zcm_t *zcm);
int    zcm_handle(zcm_t *zcm); /* returns 0 normally, and -1 when an error occurs. */

/* Non-Blocking Mode Only: Functions checking and dispatching messages */
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
