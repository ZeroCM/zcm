#define _GNU_SOURCE
#include <zcm/zcm.h>
#include <zcm/url.h>
#include <zcm/transport_registrar.h>
#include <pthread.h>
#include <sched.h>
#include "util.h"
#include "zcmtypes/blob_t.h"

#define VERBOSE 1

#define URL_IPCSHM "ipcshm://test_direct?mtu=1000&depth=8"

typedef struct state state_t;
struct state
{
    bool running;
    zcm_trans_t *trans;
};

typedef struct data data_t;
struct data
{
    i64 send_time;
    u64 seq;
};
static data_t DATA[1];

static void pin_cpu_core(int core_id)
{
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (core_id < 0 || core_id >= num_cores) {
        FAIL("Invalid core id: num_cores=%d", num_cores);
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

static void *publish_thread_direct(void *usr)
{
    pin_cpu_core(2);

    static u64 seq = 0;

    state_t *st = (state_t*)usr;
    zcm_trans_t *trans = st->trans;

    while (st->running) {
        usleep((i64)100e3); // 10 hz

        DATA->send_time = wallclock();
        DATA->seq = seq++;

        zcm_msg_t msg[1];
        msg->utime = 0;
        msg->channel = "BLOB";
        msg->len = sizeof(data_t);
        msg->buf = (u8*)DATA;

        int ret = zcm_trans_sendmsg(trans, *msg);
        if (ret != ZCM_EOK) {
            FAIL("Failed to publish!");
        }
    }

    return NULL;
}

static void *handle_thread_direct(void *usr)
{
    pin_cpu_core(3);

    state_t *st = (state_t*)usr;
    zcm_trans_t *trans = st->trans;
    size_t recv_count = 0;

    zcm_trans_recvmsg_enable(trans, "BLOB", true);

    zcm_msg_t msg[1];
    while (1) {
        uint64_t drops = 0;
        int ret = zcm_trans_query_drops(trans, &drops);
        assert(ret == ZCM_EOK);
        printf("status | recv: %lu drop: %lu\n", recv_count, drops);

        // consume all
        while (1) {
            int ret = zcm_trans_recvmsg(trans, msg, 10);
            if (ret != ZCM_EOK) break;
            recv_count++;
        }
        usleep(1e6); // sleep to stall and drop
    }

    st->running = false;
    return NULL;
}

int main(int argc, char *argv[])
{
    zcm_url_t* u = zcm_url_create(URL_IPCSHM);
    if (!u) FAIL("Failed to create url");

    const char* protocol = zcm_url_protocol(u);

    zcm_trans_create_func* creator = zcm_transport_find(protocol);
    if (!creator) FAIL("Failed to find transport type by url");

    char *errmsg;
    zcm_trans_t* trans = creator(u, &errmsg);
    if (!trans) FAIL("Failed to create transport: %s", errmsg);

    state_t st[1] = {};
    st->running = true;
    st->trans = trans;

    pthread_t pub_thread, hdl_thread;
    pthread_create(&pub_thread, NULL, publish_thread_direct, st);
    pthread_create(&hdl_thread, NULL, handle_thread_direct, st);

    usleep(10e6);
    st->running = true;

    pthread_join(pub_thread, NULL);
    pthread_join(hdl_thread, NULL);

    zcm_trans_destroy(trans);
    return 1;
}
