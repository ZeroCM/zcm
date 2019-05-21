#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/url.h>
#include <zcm/transport_registrar.h>
#include <string.h>
#include <unistd.h>

#define ENSURE(v) do {\
  if (!(v)) { \
      fprintf(stderr, "ENSURE: failed for '" #v "' at %s:%d\n", __FILE__, __LINE__); \
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

static zcm_trans_methods_t pub_blockforever_methods;
static zcm_trans_t pub_blockforever_trans;
static int pub_blockforever_sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
{
    // XXX this is probably a bug in the transport api
    //     sendmsg() is allowed to block indefinately. This means that it might be impossible
    //     to join the sending thread. We work-around this issue by setting a very high delay.
    // XXX this should be fixed in the transport api spec
    usleep(100000);
    return ZCM_EOK;
}
static zcm_trans_t *transport_pub_blockforever_create(zcm_url_t *url)
{
    init_generic(&pub_blockforever_trans, &pub_blockforever_methods);
    pub_blockforever_methods.sendmsg = pub_blockforever_sendmsg;
    return &pub_blockforever_trans;
}

static zcm_trans_methods_t sub_methods;
static zcm_trans_t sub_trans;
static int sub_recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)
{
    if (0 == strcmp(channel, "CANNOT_SUB")) {
        return ZCM_EUNKNOWN;
    }

    if (0 == strcmp(channel, "CANNOT_UNSUB")) {
        return enable ? ZCM_EOK : ZCM_EUNKNOWN;
    }

    return ZCM_EOK;
}
static zcm_trans_t *transport_sub_create(zcm_url_t *url)
{
    init_generic(&sub_trans, &sub_methods);
    sub_methods.recvmsg_enable = sub_recvmsg_enable;
    return &sub_trans;
}

static void register_transports(void)
{
    ENSURE(zcm_transport_register(
        "test-fail", "", transport_fail_create));

    ENSURE(zcm_transport_register(
        "test-generic", "", transport_generic_create));

    ENSURE(zcm_transport_register(
        "test-pub-blockforever", "", transport_pub_blockforever_create));

    ENSURE(zcm_transport_register(
        "test-sub", "", transport_sub_create));
}

static void test_fail_construct(void)
{
    ENSURE(NULL == zcm_create_from_url("test-fail"));
    ENSURE(NULL == zcm_create_trans(NULL));

    zcm_t zcm;

    memset(&zcm, 0, sizeof(zcm));
    ENSURE(-1 == zcm_init(&zcm, "test-fail"));
    ENSURE(ZCM_ECONNECT == zcm_errno(&zcm));

    memset(&zcm, 0, sizeof(zcm));
    ENSURE(-1 == zcm_init_trans(&zcm, NULL));
    ENSURE(ZCM_ECONNECT == zcm_errno(&zcm));
}

static void test_publish(void)
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
        ENSURE(ZCM_EOK == zcm_errno(&zcm));

        /* channel size 1 passed the limit */
        channel[ZCM_CHANNEL_MAXLEN] = 'A';
        channel[ZCM_CHANNEL_MAXLEN+1] = '\0';
        ENSURE(-1 == zcm_publish(&zcm, channel, &data, 1));
        ENSURE(ZCM_EINVALID == zcm_errno(&zcm));
    }

    // Test data size limit checking
    {
        const char *channel = "FOO";
        char *data = malloc(GENERIC_MTU+1);

        /* data size at limit */
        ENSURE(0 == zcm_publish(&zcm, channel, data, GENERIC_MTU));
        ENSURE(ZCM_EOK == zcm_errno(&zcm));

        /* data size 1 passed the limit */
        ENSURE(-1 == zcm_publish(&zcm, channel, data, GENERIC_MTU+1));
        ENSURE(ZCM_EINVALID == zcm_errno(&zcm));

        free(data);
    }

    zcm_stop(&zcm);
    zcm_cleanup(&zcm);
}

static void test_publish_msgdrop(void)
{
    zcm_t zcm;
    zcm_init(&zcm, "test-pub-blockforever");
    zcm_start(&zcm);

    // NOTE: We assume that 100000 publish calls are enought to overflow the send buffer
    const int MAX_PUBS = 100000;
    for (int i = 0; i < MAX_PUBS; i++) {
        char data = 'a';
        int ret = zcm_publish(&zcm, "CHANNEL", &data, 1);
        if (ret == -1) {
            ENSURE(ZCM_EAGAIN == zcm_errno(&zcm));
            goto done;
        }
    }

    FAIL("Failed to get an error return code from zcm_publish()");

 done:
    zcm_stop(&zcm);
    zcm_cleanup(&zcm);
}

static void test_sub(void)
{
    zcm_t zcm;
    zcm_sub_t *sub;
    zcm_init(&zcm, "test-sub");

    /* can't subscribe */
    ENSURE(NULL == zcm_subscribe(&zcm, "CANNOT_SUB", NULL, NULL));

    /* can't unsubscribe */
    ENSURE(NULL != (sub=zcm_subscribe(&zcm, "CANNOT_UNSUB", NULL, NULL)));
    ENSURE(-1 == zcm_unsubscribe(&zcm, sub));

    /* all good */
    ENSURE(NULL != (sub=zcm_subscribe(&zcm, "ALL_GOOD", NULL, NULL)));
    ENSURE(0 == zcm_unsubscribe(&zcm, sub));

    zcm_cleanup(&zcm);
}

int main(void)
{
    register_transports();

    test_fail_construct();
    test_publish();
    test_publish_msgdrop();
    test_sub();
}
