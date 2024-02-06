#include <zcm/zcm.h>
#include <zcm/url.h>
#include <zcm/transport_registrar.h>
#include <pthread.h>
#include "util.h"
#include "zcmtypes/blob_t.h"

#define VERBOSE 0
#define LIMIT 100

#define URL_IPC "ipc"
#define URL_IPCSHM "ipcshm://test_many?mtu=10000000&depth=4"
#define MAX_SZ (10*(1<<20))

static size_t TESTCASES[] = {
                             100,
                             1000,
                             10000,
                             100000,
                             1000000,
                             10000000,
};

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
    test_result_t res[1];
};

typedef struct data data_t;
struct data
{
    i64 send_time;
    size_t msg_size;
    __attribute__((aligned(alignof(u64)))) char BLOB[MAX_SZ];
};
static data_t DATA[1];

static void blob_init(void)
{
    assert(sizeof(DATA->BLOB) % sizeof(u64) == 0);

    size_t n = sizeof(DATA->BLOB) / sizeof(u64);
    u64 *ptr = (u64*)&DATA->BLOB;
    for (size_t i = 0; i < n; i++) {
        ptr[i] = hash_u64(i+1);
    }
}

static void *publish_thread(void *usr)
{
    state_t *st = (state_t*)usr;
    zcm_t *zcm = st->zcm;

    usleep((i64)1e6); // wait for subscribe side to init (if needed)

    while (st->running) {
        usleep((i64)1e3); // 1 millis

        blob_t msg[1];
        msg->send_time = wallclock();
        msg->n = st->msg_size;
        msg->data = (i8*)DATA->BLOB;

        int ret = blob_t_publish(zcm, "BLOB", msg);
        if (ret != 0) {
            FAIL("Failed to publish!");
        }
    }

    return NULL;
}

static void handler(const zcm_recv_buf_t *rbuf, const char *channel, const blob_t *msg, void *usr)
{
    state_t *st = (state_t*)usr;

    i64 now = wallclock();

    if (VERBOSE) {
        printf("Message:\n");
        printf("  channel        :    %s\n", channel);
        printf("  msg->send_time :    %ld\n", msg->send_time);
        printf("  msg->n         :    %ld\n", msg->n);
        printf("Send delta: %.f micros\n", (now - msg->send_time)/1000.);
    }

    st->res->num_messages++;
    st->res->total_delay += now - msg->send_time;
}

static void *handle_thread(void *usr)
{
    state_t *st = (state_t*)usr;
    zcm_t *zcm = st->zcm;

    blob_t_subscribe(zcm, "BLOB", handler, st);
    while (st->res->num_messages < LIMIT) {
        zcm_handle(zcm);
    }

    st->running = false;
    return NULL;
}

static test_result_t run_test(const char *url, size_t msg_size)
{
    zcm_t *zcm = zcm_create(url);
    if (!zcm) FAIL("Failed to create zcm");

    state_t st[1] = {};
    st->zcm = zcm;
    st->running = true;
    st->msg_size = msg_size;

    pthread_t pub_thread, hdl_thread;
    pthread_create(&pub_thread, NULL, publish_thread, st);
    pthread_create(&hdl_thread, NULL, handle_thread, st);

    pthread_join(pub_thread, NULL);
    pthread_join(hdl_thread, NULL);

    zcm_destroy(zcm);
    return *st->res;
}

static void *publish_thread_direct(void *usr)
{
    state_t *st = (state_t*)usr;
    zcm_trans_t *trans = st->trans;

    usleep((i64)1e6); // wait for subscribe side to init (if needed)

    while (st->running) {
        usleep((i64)1e3); // 10 millis

        DATA->send_time = wallclock();
        DATA->msg_size = st->msg_size;

        zcm_msg_t msg[1];
        msg->utime = 0;
        msg->channel = "BLOB";
        msg->len = offsetof(data_t, BLOB) + st->msg_size;
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
    state_t *st = (state_t*)usr;
    zcm_trans_t *trans = st->trans;

    zcm_trans_recvmsg_enable(trans, "BLOB", true);

    zcm_msg_t msg[1];
    while (st->res->num_messages < LIMIT) {
        int ret = zcm_trans_recvmsg(trans, msg, 0);
        if (ret != ZCM_EOK) continue;

        i64 now = wallclock();
        data_t *data = (data_t*)msg->buf;

        if (VERBOSE) {
            printf("Message:\n");
            printf("  channel        :    %s\n", msg->channel);
            printf("  msg->send_time :    %ld\n", data->send_time);
            printf("  msg->n         :    %zu\n", data->msg_size);
            printf("Send delta: %.f micros\n", (now - data->send_time)/1000.);
        }

        st->res->num_messages++;
        st->res->total_delay += now - data->send_time;
    }

    st->running = false;
    return NULL;
}

static test_result_t run_test_direct(const char *url, size_t msg_size)
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
    if (argc != 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        return 1;
    }

    blob_init();

    /* size_t msg_size = 1000; */
    /* test_result_t res_ipc_direct = run_test_direct(URL_IPC, msg_size); */
    /* (void)res_ipc_direct; */
    /* exit(2); */

    printf("%10s | %-15s | %-15s | %-15s | %-15s\n", "", "ipc", "ipcshm", "ipc_direct", "ipcshm_direct");
    printf("---------------------------------------------------------------------------------\n");


    for (size_t i = 0; i < ARRAY_SIZE(TESTCASES); i++) {
        size_t msg_size = TESTCASES[i];
        test_result_t res_ipc = run_test(URL_IPC, msg_size);
        test_result_t res_ipcshm = run_test(URL_IPCSHM, msg_size);
        test_result_t res_ipc_direct = run_test_direct(URL_IPC, msg_size);
        test_result_t res_ipcshm_direct = run_test_direct(URL_IPCSHM, msg_size);

        double dt_ipc = (double)res_ipc.total_delay / res_ipc.num_messages / 1000.0;
        double dt_ipcshm = (double)res_ipcshm.total_delay / res_ipcshm.num_messages / 1000.0;
        double dt_ipc_direct = (double)res_ipc_direct.total_delay / res_ipc_direct.num_messages / 1000.0;
        double dt_ipcshm_direct = (double)res_ipcshm_direct.total_delay / res_ipcshm_direct.num_messages / 1000.0;

        printf("%10zu | %8.0f micros | %8.0f micros | %8.0f micros | %8.0f micros\n", msg_size, dt_ipc, dt_ipcshm, dt_ipc_direct, dt_ipcshm_direct);
    }

    return 0;
}
