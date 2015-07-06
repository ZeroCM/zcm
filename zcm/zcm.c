#include "zcm.h"
#include <zmq.h>
#include <string.h>
#include <assert.h>

#define PUB_ADDR "ipc:///tmp/pub"
#define SUB_ADDR "ipc:///tmp/sub"
#define ZMQ_IO_THREADS 1

struct zcm
{
    void *ctx;
    void *pubsock;
    void *subsock;
};

zcm_t *zcm_create(void)
{
    zcm_t *zcm = calloc(1, sizeof(*zcm));
    zcm->ctx = zmq_init(ZMQ_IO_THREADS);
    zcm->pubsock = zmq_socket(zcm->ctx, ZMQ_PUB);
    zcm->subsock = zmq_socket(zcm->ctx, ZMQ_SUB);

    // XXX need to do error checking!
    zmq_connect(zcm->pubsock, PUB_ADDR);
    //zmq_connect(zcm->subsock, SUB_ADDR);

    return zcm;
}

void zcm_destroy(zcm_t *zcm)
{
    // TODO
}

int zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len)
{
    size_t channel_len = strlen(channel);
    int rc;

    rc = zmq_send(zcm->pubsock, channel, channel_len, ZMQ_SNDMORE);
    assert(rc == channel_len);
    rc = zmq_send(zcm->pubsock, data, len, 0);
    assert(rc == len);

    return 0;
}

int zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
    return 0;
}
