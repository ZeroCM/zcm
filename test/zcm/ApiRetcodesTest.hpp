#ifndef APIRETCODESTTEST_HPP
#define APIRETCODESTTEST_HPP

#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/url.h>
#include <zcm/transport_registrar.h>
#include <string.h>
#include <unistd.h>

#include "cxxtest/TestSuite.h"

using namespace std;

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
int    generic_update(zcm_trans_t *zt) { TS_FAIL("update should never be called on a blocking transport"); return ZCM_EINVALID; }
int    generic_recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, unsigned timeout) { usleep(timeout*1000); return ZCM_EAGAIN; }
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

static zcm_trans_t *transport_fail_create(zcm_url_t *url, char **opt_errmsg)
{
    return NULL;
}

static zcm_trans_methods_t generic_methods;
static zcm_trans_t generic_trans;
static zcm_trans_t *transport_generic_create(zcm_url_t *url, char **opt_errmsg)
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
static zcm_trans_t *transport_pub_blockforever_create(zcm_url_t *url, char **opt_errmsg)
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
static zcm_trans_t *transport_sub_create(zcm_url_t *url, char **opt_errmsg)
{
    init_generic(&sub_trans, &sub_methods);
    sub_methods.recvmsg_enable = sub_recvmsg_enable;
    return &sub_trans;
}

class ApiRetcodesTest : public CxxTest::TestSuite
{
    bool transportsAreRegistered = false;
  public:

    void setUp() override {
        if(transportsAreRegistered) {
            TS_ASSERT(!zcm_transport_register("test-fail", "", transport_fail_create));
            TS_ASSERT(!zcm_transport_register("test-generic", "", transport_generic_create));
            TS_ASSERT(!zcm_transport_register("test-pub-blockforever", "", transport_pub_blockforever_create));
            TS_ASSERT(!zcm_transport_register("test-sub", "", transport_sub_create));
        } else {
            TS_ASSERT(zcm_transport_register("test-fail", "", transport_fail_create));
            TS_ASSERT(zcm_transport_register("test-generic", "", transport_generic_create));
            TS_ASSERT(zcm_transport_register("test-pub-blockforever", "", transport_pub_blockforever_create));
            TS_ASSERT(zcm_transport_register("test-sub", "", transport_sub_create));
            transportsAreRegistered = true;
        }
    }
    void tearDown() override {}

    void testFailConstruct(void)
    {
        zcm_t* zcm_ptr;
        TS_ASSERT_EQUALS(ZCM_ECONNECT, zcm_try_create(&zcm_ptr, "test-fail", NULL));
        TS_ASSERT_EQUALS(nullptr, zcm_ptr);

        zcm_try_create_from_trans(&zcm_ptr, NULL);
        TS_ASSERT_EQUALS(nullptr, zcm_ptr);

        zcm_t zcm;

        memset(&zcm, 0, sizeof(zcm));
        TS_ASSERT_EQUALS(ZCM_ECONNECT, zcm_init(&zcm, "test-fail", NULL));

        memset(&zcm, 0, sizeof(zcm));


        TS_ASSERT_EQUALS(ZCM_EINVALID, zcm_try_create_from_trans(&zcm_ptr, NULL));
    }

    void testPublish(void)
    {
        zcm_t zcm;
        zcm_init(&zcm, "test-generic", NULL);
        zcm_start(&zcm);

        // Test channel size limit checking
        {
            char channel[ZCM_CHANNEL_MAXLEN+2];
            uint8_t data = 'A';

            /* channel size at limit */
            memset(channel, 'A', ZCM_CHANNEL_MAXLEN);
            channel[ZCM_CHANNEL_MAXLEN] = '\0';
            TS_ASSERT_EQUALS(ZCM_EOK, zcm_publish(&zcm, channel, &data, 1));

            /* channel size 1 passed the limit */
            channel[ZCM_CHANNEL_MAXLEN] = 'A';
            channel[ZCM_CHANNEL_MAXLEN+1] = '\0';
            TS_ASSERT_EQUALS(ZCM_EINVALID, zcm_publish(&zcm, channel, &data, 1));
        }

        // Test data size limit checking
        {
            const char *channel = "FOO";
            uint8_t *data = (uint8_t*) malloc(GENERIC_MTU+1);

            /* data size at limit */
            TS_ASSERT_EQUALS(ZCM_EOK, zcm_publish(&zcm, channel, data, GENERIC_MTU));

            /* data size 1 passed the limit */
            TS_ASSERT_EQUALS(ZCM_EINVALID, zcm_publish(&zcm, channel, data, GENERIC_MTU+1));

            free(data);
        }

        zcm_stop(&zcm);
        zcm_cleanup(&zcm);
    }

    void testSub(void)
    {
        zcm_t zcm;
        zcm_sub_t *sub;
        zcm_init(&zcm, "test-sub", NULL);

        /* can't subscribe */
        TS_ASSERT_EQUALS(nullptr, zcm_subscribe(&zcm, "CANNOT_SUB", NULL, NULL));

        /* can't unsubscribe */
        TS_ASSERT(NULL != (sub=zcm_subscribe(&zcm, "CANNOT_UNSUB", NULL, NULL)));
        TS_ASSERT_EQUALS(ZCM_EINVALID, zcm_unsubscribe(&zcm, sub));

        /* all good */
        TS_ASSERT(NULL != (sub=zcm_subscribe(&zcm, "ALL_GOOD", NULL, NULL)));
        TS_ASSERT_EQUALS(ZCM_EOK, zcm_unsubscribe(&zcm, sub));

        zcm_cleanup(&zcm);
    }
};


#endif // APIRETCODESTTEST_HPP
