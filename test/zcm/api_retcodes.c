#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/url.h>
#include <zcm/transport_registrar.h>
#include <string.h>
#include <unistd.h>

#define ENSURE(v) do {\
  if (!(v)) { \
    fprintf(stderr, "ENSURE: failed for '" #v "'\n");  \
    exit(1);                                          \
  }\
} while(0)

#define FAIL(msg) do {                          \
  fprintf(stderr, "FAIL: " msg "\n");             \
  exit(1);                                        \
} while(0)

#define GENERIC_MTU 256
int zcm_msg_validate(zcm_msg_t msg)
{
    if (strlen(msg.channel) > ZCM_CHANNEL_MAXLEN) return ZCM_EINVALID;
    if (msg.len > GENERIC_MTU) return ZCM_EINVALID;
    return ZCM_EOK;
}
size_t generic_get_mtu(zcm_trans_t *zt) { return GENERIC_MTU; }
int    generic_sendmsg(zcm_trans_t *zt, zcm_msg_t msg) { return zcm_msg_validate(msg); }
int    generic_recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable) { return ZCM_EOK; }
int    generic_update(zcm_trans_t *zt) { FAIL("update should never be called on a blocking transport"); }
int    generic_recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout) { usleep(timeout*1000); return ZCM_EAGAIN; }
void   generic_destroy(zcm_trans_t *zt) {}

void init_generic(zcm_trans_t *zt, zcm_trans_methods_t *methods)
{
    methods->get_mtu = generic_get_mtu;
    methods->sendmsg = generic_sendmsg;
    methods->recvmsg_enable = generic_recvmsg_enable;
    methods->recvmsg = generic_recvmsg;
    methods->update = generic_update;
    methods->destroy = generic_destroy;

    zt->trans_type = ZCM_BLOCKING;
    zt->vtbl = methods;
}

static zcm_trans_t *transport_fail_create(zcm_url_t *url)
{
    return NULL;
}

static zcm_trans_methods_t generic_methods;
static zcm_trans_t generic_trans;
static zcm_trans_t *transport_generic_create(zcm_url_t *url)
{
    init_generic(&generic_trans, &generic_methods);
    return &generic_trans;
}

static void register_transports(void)
{
    ENSURE(zcm_transport_register(
        "test-fail", "", transport_fail_create));

    ENSURE(zcm_transport_register(
        "test-generic", "", transport_generic_create));
}

static void test_fail_construct(void)
{
    ENSURE(NULL == zcm_create("test-fail"));
    ENSURE(NULL == zcm_create_trans(NULL));

    zcm_t zcm;
    ENSURE(-1 == zcm_init(&zcm, "test-fail"));
    ENSURE(-1 == zcm_init_trans(&zcm, NULL));
}

static void test_send(void)
{
    zcm_t zcm;
    zcm_init(&zcm, "test-generic");
    zcm_start(&zcm);

    // Test channel size limit checking
    {
        char channel[ZCM_CHANNEL_MAXLEN+2];
        char data = 'A';

        /* channel size at limit */
        memset(channel, 'A', ZCM_CHANNEL_MAXLEN);
        channel[ZCM_CHANNEL_MAXLEN] = '\0';
        ENSURE(0 == zcm_publish(&zcm, channel, &data, 1));

        /* channel size 1 passed the limit */
        channel[ZCM_CHANNEL_MAXLEN] = 'A';
        channel[ZCM_CHANNEL_MAXLEN+1] = '\0';
        ENSURE(-1 == zcm_publish(&zcm, channel, &data, 1));
    }

    // Test data size limit checking
    {
        const char *channel = "FOO";
        char *data = malloc(GENERIC_MTU+1);

        /* data size at limit */
        ENSURE(0 == zcm_publish(&zcm, channel, data, GENERIC_MTU));

        /* data size 1 passed the limit */
        ENSURE(-1 == zcm_publish(&zcm, channel, data, GENERIC_MTU+1));

        free(data);
    }

    zcm_stop(&zcm);
    zcm_cleanup(&zcm);
}

int main(void)
{
    register_transports();

    test_fail_construct();
    test_send();
}
