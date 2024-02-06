#define _GNU_SOURCE
#include <zcm/zcm.h>
#include <zcm/url.h>
#include <zcm/transport_registrar.h>
#include <pthread.h>
#include <sched.h>
#include "util.h"
#include "zcmtypes/blob_t.h"

#define VERBOSE 1

#define URL_IPC "ipc"
#define URL_IPCSHM "ipcshm://test_direct?mtu=1000000&depth=8"

typedef struct test_result test_result_t;
struct test_result
{
    size_t num_messages;
    i64 total_delay; // nanos
};

typedef struct state state_t;
struct state
{
    union {
        zcm_t * zcm;
        zcm_trans_t * trans;
    };
    volatile bool running;
    size_t msg_size;
    size_t limit;
    test_result_t res[1];
};

typedef struct data data_t;
struct data
{
    i64 send_time;
    size_t msg_size;
    __attribute__((aligned(alignof(u64)))) char BLOB[2*(1<<20)];
};
static data_t *DATA = NULL;

static void blob_init(void)
{
    DATA = (data_t*)malloc(sizeof(data_t));
    assert(sizeof(DATA->BLOB) % sizeof(u64) == 0);

    size_t n = sizeof(DATA->BLOB) / sizeof(u64);
    u64 *ptr = (u64*)&DATA->BLOB;
    for (size_t i = 0; i < n; i++) {
        ptr[i] = hash_u64(i+1);
    }
}

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

    state_t *st = (state_t*)usr;
    zcm_trans_t *trans = st->trans;

    usleep((i64)1e6); // wait for subscribe side to init (if needed)

    while (st->running) {
        usleep((i64)5e3); // 5 millis

        DATA->send_time = wallclock();
        DATA->msg_size = st->msg_size;

        zcm_msg_t msg[1];
        msg->utime = 0;
        msg->channel = "BLOB";
        msg->len = offsetof(data_t, BLOB) + st->msg_size;
        msg->buf = (u8*)DATA;

        i64 start = wallclock();
        int ret = zcm_trans_sendmsg(trans, *msg);
        i64 dt = wallclock() - start;

        if (VERBOSE) {
            printf("Sendmsg latency: %ld\n", dt);
            printf("\n");
        }

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

    zcm_trans_recvmsg_enable(trans, "BLOB", true);

    zcm_msg_t msg[1];
    while (st->res->num_messages < st->limit) {
        i64 start = wallclock();
        int ret = zcm_trans_recvmsg(trans, msg, 10);
        if (ret != ZCM_EOK) continue;
        i64 dt = wallclock() - start;

        i64 now = wallclock();
        data_t *data = (data_t*)msg->buf;

        if (VERBOSE) {
            printf("Recvmsg latency: %ld\n", dt);
            printf("\n");
            printf("Message:\n");
            printf("  channel        :    %s\n", msg->channel);
            printf("  msg->send_time :    %ld\n", data->send_time);
            printf("  msg->n         :    %zu\n", data->msg_size);
            printf("\n");
            printf("Message latency: %ld\n", now - data->send_time);
            printf("\n");
        }

        st->res->num_messages++;
        st->res->total_delay += now - data->send_time;
    }

    st->running = false;
    return NULL;
}

static test_result_t run_test_direct(const char *url, size_t msg_size, size_t limit)
{
    zcm_url_t* u = zcm_url_create(url);
    if (!u) FAIL("Failed to create url");

    const char* protocol = zcm_url_protocol(u);

    zcm_trans_create_func* creator = zcm_transport_find(protocol);
    if (!creator) FAIL("Failed to find transport type by url");

    char *errmsg;
    zcm_trans_t* trans = creator(u, &errmsg);
    if (!trans) FAIL("Failed to create transport: %s", errmsg);

    state_t st[1] = {};
    st->trans = trans;
    st->running = true;
    st->msg_size = msg_size;
    st->limit = limit;

    pthread_t pub_thread, hdl_thread;
    pthread_create(&pub_thread, NULL, publish_thread_direct, st);
    pthread_create(&hdl_thread, NULL, handle_thread_direct, st);

    pthread_join(pub_thread, NULL);
    pthread_join(hdl_thread, NULL);

    zcm_trans_destroy(trans);
    return *st->res;

}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <limit>\n", argv[0]);
        return 1;
    }
    size_t limit = atoll(argv[1]);

    blob_init();

    size_t msg_size = 100;
    test_result_t res = run_test_direct(URL_IPCSHM, msg_size, limit);
    double dt = (double)res.total_delay / res.num_messages;

    printf("%10zu | %8.0f nanos\n", msg_size, dt);

    return 0;
}
