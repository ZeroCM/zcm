#ifndef _ZCM_H
#define _ZCM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Version: M.m.u
 *   M: Major
 *   m: Minor
 *   u: Micro
 */
#define ZCM_MAJOR_VERSION 2
#define ZCM_MINOR_VERSION 0
#define ZCM_MICRO_VERSION 0

#include <stdint.h>

#include <assert.h>
#define ZCM_ASSERT(X) assert(X)

#ifndef ZCM_EMBEDDED
#include "eventlog.h"
#endif

/* Important hardcoded values */
#define ZCM_CHANNEL_MAXLEN 32
enum zcm_type
{
    ZCM_BLOCKING,
    ZCM_NONBLOCKING
};

#define ZCM_RETURN_CODES \
    X(ZCM_EOK,               0, "Okay, no errors"                       ) \
    X(ZCM_EINVALID,         -1, "Invalid arguments"                     ) \
    X(ZCM_EAGAIN  ,         -2, "Resource unavailable, try again"       ) \
    X(ZCM_ECONNECT,         -3, "Transport connection failed"           ) \
    X(ZCM_EINTR   ,         -4, "Operation was unexpectedly interrupted") \
    X(ZCM_EUNKNOWN,         -5, "Unknown error"                         ) \
    X(ZCM_EMEMORY,          -6, "Out of memory"                         ) \
    X(ZCM_EUNSUPPORTED,     -7, "Unsupported functionality"             ) \
    X(ZCM_NUM_RETURN_CODES,  8, "Invalid return code"                   )

/* Return codes */
enum zcm_return_codes
{
#define X(n, v, s) n = v,
    ZCM_RETURN_CODES
#undef X
};

/* Forward typedef'd structs */
typedef struct zcm_trans_t    zcm_trans_t;
typedef struct zcm_t          zcm_t;
typedef struct zcm_recv_buf_t zcm_recv_buf_t;
typedef struct zcm_sub_t      zcm_sub_t;

/* Generic message handler function type */
typedef void (*zcm_msg_handler_t)(const zcm_recv_buf_t* rbuf, const char* channel,
                                  void* usr);

/* Note: some language bindings depend on the specific memory layout
 *       of ZCM structures. If you change these, be sure to update
 *       language bindings to match. */

/* Primary ZCM object that handles all top-level interactions including
   delegation between blocking and non-blocking interfaces */
struct zcm_t
{
    enum zcm_type type;
    void*         impl;
};

/* ZCM Receive buffer for one message */
struct zcm_recv_buf_t
{
    int64_t  recv_utime;
    zcm_t*   zcm;
    uint8_t* data; /* NOTE: do not free, the library manages this memory */
    uint32_t data_size;
};

#ifndef ZCM_EMBEDDED
int zcm_retcode_name_to_enum(const char* zcm_retcode_name);
#endif

/* Standard create/destroy functions. These will malloc() and free() the zcm_t object. */
#ifndef ZCM_EMBEDDED
zcm_t* zcm_create(const char* url);
// If opt_errmsg pointer is provided, it will be set to new memory that you
// must deallocate
int zcm_try_create(zcm_t** z, const char* url, char** opt_errmsg);
#endif
zcm_t* zcm_create_from_trans(zcm_trans_t* zt);
int    zcm_try_create_from_trans(zcm_t** z, zcm_trans_t* zt);
void   zcm_destroy(zcm_t* zcm);

#ifndef ZCM_EMBEDDED
/* Initialize a zcm object allocated by caller
   Returns ZCM_EOK on success, error code on failure
   Optionally sets the out-param opt_errmsg with an error message (called is responsible
   for free())*/
int zcm_init(zcm_t* zcm, const char* url, char** opt_errmsg);
#endif

/* Initialize a zcm instance allocated by caller using a transport provided by caller
   Returns ZCM_EOK on success, error code on failure */
int zcm_init_from_trans(zcm_t* zcm, zcm_trans_t* zt);

/* Cleanup a zcm object allocated by caller */
void zcm_cleanup(zcm_t* zcm);

/* Returns the error string from the error number */
const char* zcm_strerrno(int err);

/* Subscribe to zcm messages
   Returns a subscription object on success, and NULL on failure */
zcm_sub_t* zcm_subscribe(zcm_t* zcm, const char* channel, zcm_msg_handler_t cb,
                         void* usr);

/* Unsubscribe to zcm messages, freeing the subscription object
   Returns ZCM_EOK on success, error code on failure */
int zcm_unsubscribe(zcm_t* zcm, zcm_sub_t* sub);

/* Publish a zcm message buffer.
 * Returns ZCM_EOK on success, error code on failure */
int zcm_publish(zcm_t* zcm, const char* channel, const uint8_t* data, uint32_t len);

/* Dispatches a single incoming message if available.
 * Returns ZCM_EOK if a message was dispatched, error code on failure.
 * timeout is in units of milliseconds */
int zcm_handle(zcm_t* zcm, unsigned timeout);

/* Dispatches messages continuously until it has no more to dispatch.
 * This may run forever, if your handlers are too slow and you receive
 * more messages in the time it takes all subscription handlers to run.
 * If you want to guarantee a return, do something like:
 * while(handle() == ZCM_EOK && ++i < n) */
int zcm_flush(zcm_t* zcm);

/* Request that the underlying transport queue at least this many messages.
 * Messages will accumulate unless:
 *  - zcm_handle() is called repeatedly
 *  - zcm_run() was called
 *  - zcm_start() was called once or since the most recent zcm_stop() */
int zcm_set_queue_size(zcm_t* zcm, unsigned num_messages);

#ifndef ZCM_EMBEDDED
/* Blocking Mode Only: Functions for controlling the message dispatch loop */
void zcm_run(zcm_t* zcm);
void zcm_start(zcm_t* zcm);
// This function may be called if zcm_run() is currently running in another thread
void zcm_stop(zcm_t* zcm);
void zcm_pause(zcm_t* zcm); /* pauses all interaction with transport */
void zcm_resume(zcm_t* zcm);

/* Write topology file to filename. Returns ZCM_EOK normally, error code on failure */
int zcm_write_topology(zcm_t* zcm, const char* name);
#endif

/* Query the drop counter on the underlying transport
   NOTE: This may be unimplemented, in which case it will return ZCM_EUNSUPPORTED
   and out_drops will be disregarded. */
int zcm_query_drops(zcm_t *zcm, uint64_t *out_drops);

/****************************************************************************/
/*    NOT FOR GENERAL USE. USED FOR LANGUAGE-SPECIFIC BINDINGS WITH VERY    */
/*                     SPECIFIC THREADING CONSTRAINTS                       */
/****************************************************************************/
/* Subscribe to zcm messages
   Returns a subscription object on success, and NULL on failure.
   Can fail to subscribe if zcm is already running */
zcm_sub_t* zcm_try_subscribe(zcm_t* zcm, const char* channel, zcm_msg_handler_t cb,
                             void* usr);
/* Unsubscribe to zcm messages, freeing the subscription object
   Returns ZCM_EOK on success, error code on failure
   Can fail to subscribe if zcm is already running */
int zcm_try_unsubscribe(zcm_t* zcm, zcm_sub_t* sub);
/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_H */
