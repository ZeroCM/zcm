#include "transport.h"
#include <zmq.h>

#include <unistd.h>
#include <dirent.h>

#include <cstdio>
#include <cstring>
#include <cassert>

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
using namespace std;

// Define this the class name you want
#define ZCM_TRANS_NAME TransportZmqIpc
#define MTU (1<<20)
#define ZMQ_IO_THREADS 1
#define IPC_ADDR_PREFIX "ipc:///tmp/zcm-channel-zmq-ipc-"

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    void *ctx;

    unordered_map<string, void*> pubsocks;
    unordered_map<string, void*> subsocks;
    string recvmsgChannel;
    char recvmsgBuffer[MTU];

    ZCM_TRANS_CLASSNAME(/* Add any methods here */)
    {
        vtbl = &methods;
        ctx = zmq_init(ZMQ_IO_THREADS);
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        // TODO shut down ZMQ correctly
    }

    string getAddress(const string& channel)
    {
        return IPC_ADDR_PREFIX+channel;
    }

    void *pubsockFindOrCreate(const string& channel)
    {
        auto it = pubsocks.find(channel);
        if (it != pubsocks.end())
            return &it->second;
        void *sock = zmq_socket(ctx, ZMQ_PUB);
        string address = getAddress(channel);
        zmq_bind(sock, address.c_str());
        pubsocks.emplace(channel, sock);
        return sock;
    }


    void *subsockFindOrCreate(const string& channel)
    {
        auto it = subsocks.find(channel);
        if (it != subsocks.end())
            return &it->second;
        void *sock = zmq_socket(ctx, ZMQ_SUB);
        string address = getAddress(channel);
        zmq_connect(sock, address.c_str());
        zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
        subsocks.emplace(channel, sock);
        return sock;
    }

    /********************** METHODS **********************/
    size_t get_mtu()
    {
        return MTU;
    }

    int sendmsg(zcm_msg_t msg)
    {
        string channel = msg.channel;
        if (channel.size() > ZCM_CHANNEL_MAXLEN)
            return ZCM_EINVALID;
        if (msg.len > MTU)
            return ZCM_EINVALID;

        void *sock = pubsockFindOrCreate(channel);
        int rc = zmq_send(sock, msg.buf, msg.len, 0);
        if (rc == (int)msg.len)
            return ZCM_EOK;
        else
            return ZCM_EUNKNOWN;
    }

    void recvmsg_enable(const char *channel, bool enable)
    {
        assert(enable && "Disabling is not supported");
        subsockFindOrCreate(channel);
    }

    int recvmsg(zcm_msg_t *msg, int timeout)
    {
        // Build up a list of poll items
        vector<zmq_pollitem_t> pitems;
        vector<string> pchannels;
        {
            pitems.resize(subsocks.size());
            int i = 0;
            for (auto& elt : subsocks) {
                auto& channel = elt.first;
                auto& sock = elt.second;
                auto *p = &pitems[i];
                memset(p, 0, sizeof(*p));
                p->socket = sock;
                p->events = ZMQ_POLLIN;
                pchannels.emplace_back(channel);
                i++;
            }
        }

        timeout = (timeout >= 0) ? timeout : -1;
        int rc = zmq_poll(pitems.data(), pitems.size(), timeout);
        if (rc >= 0) {
            for (size_t i = 0; i < pitems.size(); i++) {
                auto& p = pitems[i];
                if (p.revents != 0) {
                    int rc = zmq_recv(p.socket, recvmsgBuffer, MTU, 0);
                    assert(0 < rc && rc < MTU);
                    recvmsgChannel = pchannels[i];
                    msg->channel = recvmsgChannel.c_str();
                    msg->len = rc;
                    msg->buf = recvmsgBuffer;
                    return ZCM_EOK;
                }
            }
        }

        return ZCM_EAGAIN;
    }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME *cast(zcm_trans_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _get_mtu(zcm_trans_t *zt)
    { return cast(zt)->get_mtu(); }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static void _recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)
    { return cast(zt)->recvmsg_enable(channel, enable); }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
    { return cast(zt)->recvmsg(msg, timeout); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_get_mtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsg_enable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    &ZCM_TRANS_CLASSNAME::_destroy,
};

zcm_trans_t *zcm_trans_ipc_create()
{
    return new ZCM_TRANS_CLASSNAME();
}
