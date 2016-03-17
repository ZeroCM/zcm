#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#include <zcm/zcm.h>

#include "util/circular.h"

#ifdef T

#define STR(X) #X
#define CONCAT(X,Y) X##_##Y
#define TMPL(X,Y) CONCAT(X,Y)

typedef struct TMPL(T, tracker_t) TMPL(T, tracker_t);
static const char* TMPL(T, tracker_name)();
static size_t TMPL(T, tracker_clear)(TMPL(T, tracker_t) *t);

// Private
struct TMPL(T, tracker_t)
{
    // How long back in time should we remember poses?
    double timeout_us;

    // Don't use messages that are off by more than this
    double  max_time_err_us;

    // True if we've warned the user about attempting to enque more than maxEnqueued
    int warned;

    // Queue keep track of incoming messages
    circular_t *queue;
    pthread_rwlock_t queue_lock;

    T* (*interpolate)(const T* const p0, const T* const p1, uint64_t utime);

    zcm_t *_zcm;
    TMPL(T, subscription_t*) subscription;

};

static void TMPL(T, tracker_handler)(const zcm_recv_buf_t* rbuf,
                                     const char* channel,
                                     const T* msg, void* usr)
{
    TMPL(T, tracker_t) *t = (TMPL(T, tracker_t*)) usr;
    T *p = TMPL(T, copy)(msg);

    pthread_rwlock_wrlock(&t->queue_lock);

    // timeout shrinkage
    T* last = *(T**)circular_front(t->queue);
    if (last) {
        if ((uint64_t)last->utime > (uint64_t)p->utime) {
            size_t size = circular_size(t->queue);
            for (size_t i = 0; i < size; ++i) {
                free(*(T**)circular_front(t->queue));
                circular_pop_front(t->queue);
            }
            fprintf(stderr, "Received " STR(T) " message on channel %s with out of "
                            "order utime. Resetting tracker...\n", channel);
        } else {
            while ((uint64_t)p->utime - (uint64_t)last->utime > t->timeout_us) {
                free(*(T**)circular_front(t->queue));
                circular_pop_front(t->queue);
            }
        }
    }

    // enqueue new message
    if ((circular_size(t->queue) == 0) ||
        ((uint64_t)p->utime > (uint64_t)(*(T**)circular_back(t->queue))->utime)) {

        if (!circular_push_back(t->queue, p)) {

            // emergency shrinkage.
            free(*(T**)circular_front(t->queue));
            circular_pop_front(t->queue);

            if (t->warned == 0) {
                fprintf(stderr, "%s ", TMPL(T, tracker_name)());
                fprintf(stderr, " queue too large. Shrinking to make room\n");
                t->warned = 1;
            }

            if (!circular_push_back(t->queue, p)) {
                free(p);

                if (t->warned == 1) {
                    fprintf(stderr, "%s ", TMPL(T, tracker_name)());
                    fprintf(stderr, " queue too large. Unable to make room\n");
                    fprintf(stderr, "%s ", TMPL(T, tracker_name)());
                    fprintf(stderr, " suppressing similar future warnings");
                    t->warned = 2;
                }
            }
        }
    }

    pthread_rwlock_unlock(&t->queue_lock);
}

static T* TMPL(T, tracker_interpolate)(const T* const m0,
                                       const T* const m1,
                                       uint64_t utime)
{
    if ((utime < (uint64_t)m0->utime) || (utime > (uint64_t)m1->utime))
        return NULL;

    // Naive interpolate: pick the one closer in time
    double err0 = utime - (uint64_t)m0->utime;
    double err1 = (uint64_t)m1->utime - utime;
    return (err0 < err1) ? TMPL(T, copy)(m0) : TMPL(T, copy)(m1);
}



// Public
static TMPL(T, tracker_t*) TMPL(T, tracker_create)(zcm_t *_zcm,
                                                   const char *channel,
                                                   const double timeout_s,
                                                   const double max_time_err_s,
                                                   const size_t max_enqueued,
                                                   T* (*interpolate)(const T* const p0,
                                                                     const T* const p1,
                                                                     uint64_t utime))
{
    if (max_enqueued == 0) return NULL;

    TMPL(T, tracker_t) *ret = (TMPL(T, tracker_t*)) malloc(sizeof(TMPL(T, tracker_t)));

    ret->timeout_us      = timeout_s * (int)1e6;
    ret->max_time_err_us = max_time_err_s * (int)1e6;
    ret->warned          = false;

    ret->queue = circular_create(max_enqueued, sizeof(T*));
    pthread_rwlock_init(&ret->queue_lock, NULL);

    ret->interpolate     = interpolate ? interpolate : TMPL(T, tracker_interpolate);
    ret->_zcm            = _zcm;
    ret->subscription    = TMPL(T, subscribe)(_zcm, channel, &TMPL(T, tracker_handler), ret);

    return ret;
}

static void TMPL(T, tracker_destroy)(TMPL(T, tracker_t) *t)
{
    TMPL(T, unsubscribe)(t->_zcm, t->subscription);

    TMPL(T, tracker_clear)(t);
    circular_destroy(t->queue);
    pthread_rwlock_destroy(&t->queue_lock);
}

static const char* TMPL(T, tracker_name)()
{
    return STR(T) " message tracker";
}

static size_t TMPL(T, tracker_clear)(TMPL(T, tracker_t) *t)
{
    pthread_rwlock_wrlock(&t->queue_lock);
    size_t size = circular_size(t->queue);
    for (size_t i = 0; i < size; ++i) {
        free(*(T**)circular_front(t->queue));
        circular_pop_front(t->queue);
    }
    pthread_rwlock_unlock(&t->queue_lock);
    return size;
}

static size_t TMPL(T, tracker_clear_before)(TMPL(T, tracker_t) *t, uint64_t utime)
{
    pthread_rwlock_wrlock(&t->queue_lock);
    size_t i = 0;
    while (circular_size(t->queue) != 0) {
        T* front = *(T**) circular_front(t->queue);
        if ((uint64_t)front->utime >= utime)
            break;
        free(front);
        circular_pop_front(t->queue);
        i++;
    }
    pthread_rwlock_unlock(&t->queue_lock);
    return i;
}

static T* TMPL(T, tracker_get)(TMPL(T, tracker_t) *t)
{
    pthread_rwlock_rdlock(&t->queue_lock);
    if (circular_size(t->queue) == 0) {
        pthread_rwlock_unlock(&t->queue_lock);
        return NULL;
    }
    T *ret = TMPL(T, copy)(*(T**)circular_back(t->queue));
    pthread_rwlock_unlock(&t->queue_lock);
    return ret;
}

static T* TMPL(T, tracker_get_utime)(TMPL(T, tracker_t) *t, uint64_t utime)
{
    T *m0 = NULL, *m1 = NULL; // two poses bracketing the desired utime

    // Search the queue for the bracketing messages
    pthread_rwlock_rdlock(&t->queue_lock);

    const T *_m0 = NULL, *_m1 = NULL;

    size_t size = circular_size(t->queue);

    for (size_t i = 0; i < size; ++i) {
        T* m = *(T**) circular_at(t->queue, i);

        if ((uint64_t)m->utime <= utime &&
            (_m0 == NULL || (uint64_t)m->utime > (uint64_t)_m0->utime))
            _m0 = m;

        if ((uint64_t)m->utime >= utime &&
            (_m1 == NULL || (uint64_t)m->utime < (uint64_t)_m1->utime))
            _m1 = m;
    }

    if (_m0 != NULL)
        m0 = TMPL(T, copy)(_m0);

    if (_m1 != NULL)
        m1 = TMPL(T, copy)(_m1);

    pthread_rwlock_unlock(&t->queue_lock);



    if (m0 && utime - (uint64_t)m0->utime > t->max_time_err_us) {
        free(m0);
        m0 = NULL;
    }

    if (m1 && (uint64_t)m1->utime - utime > t->max_time_err_us) {
        free(m1);
        m1 = NULL;
    }

    if (m0 && m1) {
        if ((uint64_t)m0->utime == (uint64_t)m1->utime) {
            free(m1);
            m1 = NULL;

            return m0;
        }

        T* elt = t->interpolate(m0, m1, utime);

        free(m0);
        free(m1);

        return elt;
    }

    if (m0) return m0;
    if (m1) return m1;

    return NULL;
}


#else
    FAILED TO DEFINE TEMPLATE TYPE FOR MESSAGE TRACKER
#endif

#ifdef __cplusplus
}
#endif
