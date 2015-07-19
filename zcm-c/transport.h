#ifndef _ZCM_TRANS_H
#define _ZCM_TRANS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

/*******************************************************************************
 * ZCM Transport Abstract Interface
 *
 *     This file provides an abstract interface for implementing ZCM on top of a
 *     variety of different transport layers. ZCM attempts to be transport
 *     agnostic by its core design, striving to only require a message type and
 *     marshalling system. By this design, we allow end-users to decide amoungst
 *     the myriad of transport layer trade-offs. For some, reliability is most
 *     critical. For others, performance dominates as a constraint. For others,
 *     ease-of-use across various platforms is important (e.g. embedded, server,
 *     and web). Whatever the need, we indeed to provide the support.
 *
 *     To these ends, this module aims to provide enough flexibility to acheive
 *     and goal the user desires. As a trade-off, the interface may be more
 *     complicated than one might intend. However, we believe this is a
 *     "necessary evil" to acheive the loft goals stated above.
 *
 *     The protocol is designed around the notion of messages. ZCM delivers full
 *     messages to the underlying transport. The underlying transport is
 *     responsible for providing a Maximum Transmission Unit (MTU) to ZCM. Any
 *     ZCM messages larger than the MTU will be *droppped* by ZCM. All messages
 *     sized within the MTU will be sent to the transport layer. However, this
 *     does *not* guarentee reliable delivery. After submitting a message to the
 *     transport, ZCM is no longer responsible for the message. Thus, reliablity
 *     guarentees are the domain of each individual transport layer. This design
 *     allows for a LOT of flexibility. Buffer over-run and congestion are
 *     handled in a similair way. The transport layer is responsible for these
 *     concerns.
 *
 *     This API is also designed with the following use-cases in mind:
 *         - minimal buffer copying
 *         - blocking and non-blocking
 *         - single and multi-threaded
 *
 *******************************************************************************
 * API Description:
 *
 *      size_t getmtu(zcm_trans_t *zt)
 *      --------------------------------------------------------------------
 *         Returns the Maximum Transmission Unit supported by this transport
 *         The transport is allowed to ignore any message above this size
 *         Users of this transport should ensure that they never attempt to
 *         send messages larger than the MTU of their chosen transport
 *
 *      zcm_buffer_t* sendmsg_start(zcm_trans_t *zt, size_t sz, bool wait)
 *      --------------------------------------------------------------------
 *         The caller to this method initiates a message send operation
 *         The caller provides the *exact* message size and receives a
 *         fresh and writable buffer from the transport layer. The caller
 *         must then populate the buffer and issue a sendmsg_finish() call.
 *         The buffer returned by this call is only valid between the
 *         sendmsg_start() and sendmsg_finish() calls. Any uses outside of
 *         this window should be considered an error. It is also considered
 *         an error for another call to sendmsg_start() before a call to
 *         sendmsg_finish(). The transport is allowed to return NULL here.
 *         In such a case, the caller should treat this as a "drop message"
 *         event. Calling this method with 'sz > get_mtu()' is an error and
 *         the transport should return NULL. If the transport layer cannot
 *         accept more data it should return NULL unless the 'wait' param
 *         is set to true. In this case, the method is allowed to block
 *         indefinately and must not return NULL (given 'sz <= get_mtu()')
 *
 *      void sendmsg_finish(zcm_trans_t *zt)
 *      --------------------------------------------------------------------
 *         The caller to this method finishes a previously started message
 *         send operation. After this call the buffer provided by
 *         sendmsg_start() is invalid and should never again be dereferenced
 *         This method should never be called without first calling
 *         sendmsg_start(). This method should *never* block. If the
 *         transport layer cannot accept more data it should return NULL from
 *         sendmsg_start().
 *
 *      int recvmsg_poll(zcm_trans_t *zt, int16_t timeout)
 *      --------------------------------------------------------------------
 *         This method polls the transport to determine if there's a message
 *         available for reciept. This method returns 1 if there's a message
 *         and returns 0 if there is not. If this method returns 1, then the
 *         next call to recvmsg_start() *must not* block. The 'timeout'
 *         parameter allows the user to specify a maximum time to wait for a
 *         message before returning 0. The time unit is in milliseconds.
 *         There are a few special values for particular usage-cases. If
 *         timeout is '0', the transport layer should return *immediately*
 *         with an answer. If timeout is '-1', the transport layer should
 *         never return until there is a message available. That is, it
 *         should block indefinately until it can return '1'
 *
 *      zcm_buffer_t *recvmsg_start(zcm_trans_t *zt)
 *      --------------------------------------------------------------------
 *         The caller to this method initiates a message recv operation
 *         The transport layer should recieve the next full message and
 *         return a buffer containing it. If the recvmsg_poll() call has
 *         returned 1 just prior to this call, it should *never* block.
 *         In any other case, this call is allowed to block *indefinately*
 *         After return, the caller is allowed to read from the returned
 *         buffer until the next call to recvmsg_finish(). After the call to
 *         recvmsg_finish() the returned buffer is invalidated and should
 *         never again be dereferenced. The user must always call
 *         recvmsg_finish() before calling recvmsg_start() again.
 *
 *      void recvmsg_finish(zcm_trans_t *zt)
 *      --------------------------------------------------------------------
 *         The caller to this method finishes a previously started message
 *         recv operation. After this call the buffer provided by
 *         recvmsg_start() is invalid and should never again be dereferenced
 *         This method should never be called without first calling
 *         recvmsg_start(). It may seem odd to do a "finish" on a recv
 *         operation after you already have the data. The intent of this
 *         method is to allow the implementation to do special buffer memory
 *         management at its own discretion. You may consider this call
 *         analogous to a free() call, but the implementation is
 *         allowed to do anything it wishes with this memroy. This method
 *         should *never* block for any reason.
 *
 *******************************************************************************
 * Design Discussion
 *
 *     This design may seem odd at first glace, esp considering the start/finish
 *     calls. One might ask why we cannot simplify the interface into a set of
 *     send/recv methods. These questions are very reasonable and this section
 *     exists to resolve many common confusions/questions.
 *
 *     By splitting send and recv into start/finish methods (i.e 4 methods), we
 *     are able to grant the transport implementation the right to full buffer
 *     management. For high-performance implementations this can be a very
 *     important feature. Consider an IPC transport using shared memory; by
 *     allowing the transport implementation to manage the buffers, it can
 *     simply return a zcm_buffer_t that points into a shared memory segment,
 *     required *zero* copying. On the other hand, this doesn't hinder any
 *     straight-forward implementations. An implementation could still malloc()
 *     on a call to 'start' and do a free() during a call to 'finish'. We
 *     just allow the implementaion to decide whether it should use the simple
 *     approach or a more sophisticated memory management technique.
 *
 *     The API also considers blocking/non-blocking issues and remains agnostic
 *     to each. The sendmsg_start() method is designed to support a blocking IO
 *     style when the 'wait' param is used. Otherwise the call never blocks,
 *     opting to return NULL to denote "can't accept more data". The
 *     recvmsg_start() method may block on any call if the caller is not careful.
 *     To discern whether it will block, the user can call recvmsg_poll(). Thus
 *     The caller can use each of these in either a blocking or non-blocking
 *     approach.
 *
 *******************************************************************************
 */

typedef struct zcm_trans_t;
typedef struct zcm_trans_methods_t zcm_trans_methods_t;

struct zcm_trans_t
{
    zcm_trans_methods_t *vtbl;
}

struct zcm_trans_methods_t
{
    size_t        (*get_mtu)         (zcm_trans_t *zt);

    zcm_buffer_t *(*sendmsg_start)  (zcm_trans_t *zt, size_t sz, bool wait);
    void          (*sendmsg_finish) (zcm_trans_t *zt);

    int           (*recvmsq_poll)   (zcm_trans_t *zt, int16_t timeout);
    zcm_buffer_t *(*recvmsg_start)  (zcm_trans_t *zt);
    void          (*recvmsg_finish) (zcm_trans_t *zt);
};

// Helper functions to make the VTbl dispatch cleaner
static inline size_t zcm_trans_get_mtu(zcm_trans_t *zt)
{ return zt->vtbl->get_mtu(zt); }

static inline zcm_buffer_t *zcm_trans_sendmsg_start(zcm_trans_t *zt, size_t sz, bool wait)
{ return zt->vtbl->sendmsg_start(zt, sz, wait); }

static inline void zcm_trans_sendmsg_finish(zcm_trans_t *zt)
{ return zt->vtbl->sendmsg_finish(zt); }

static inline int zcm_trans_recvmsg_poll(zcm_trans_t *zt, int16_t timeout)
{ return zt->vtbl->recvmsg_poll(zt, timeout); }

static inline zcm_buffer_t *zcm_trans_recvmsg_start(zcm_trans_t *zt)
{ return zt->vtbl->recvmsg_start(zt); }

static inline void zcm_trans_recvmsg_finish(zcm_trans_t *zt)
{ return zt->vtbl->recvmsg_finish(zt); }


#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANS_H */
