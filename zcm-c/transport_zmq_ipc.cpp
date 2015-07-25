#include "transport.h"
#include "debug.hpp"
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
#define ZCM_TRANS_CLASSNAME TransportZmqIpc
#define MTU (1<<20)
#define ZMQ_IO_THREADS 1
#define NAME_PREFIX "zcm-channel-zmq-ipc-"
#define IPC_ADDR_PREFIX "ipc:///tmp/" NAME_PREFIX

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    void *ctx;

    unordered_map<string, void*> pubsocks;
    unordered_map<string, void*> subsocks;
    string recvmsgChannel;
    char recvmsgBuffer[MTU];
    bool recvAllChannels = false;

    // Mutex used to protect 'subsocks' while allowing
    // recvmsgEnable() and recvmsg() to be called
    // concurrently
    mutex mut;

    ZCM_TRANS_CLASSNAME()
    {
        vtbl = &methods;
        ctx = zmq_init(ZMQ_IO_THREADS);
        assert(ctx != nullptr);
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        // TODO shut down ZMQ correctly
    }

    string getAddress(const string& channel)
    {
        return IPC_ADDR_PREFIX+channel;
    }

    // May return null if it cannot create a new pubsock
    void *pubsockFindOrCreate(const string& channel)
    {
        auto it = pubsocks.find(channel);
        if (it != pubsocks.end())
            return it->second;
        void *sock = zmq_socket(ctx, ZMQ_PUB);
        if (sock == nullptr) {
            ZCM_DEBUG("failed to create pubsock: %s", zmq_strerror(errno));
            return nullptr;
        }
        string address = getAddress(channel);
        int rc = zmq_bind(sock, address.c_str());
        if (rc == -1) {
            ZCM_DEBUG("failed to bind pubsock: %s", zmq_strerror(errno));
            return nullptr;
        }
        pubsocks.emplace(channel, sock);
        return sock;
    }

    // May return null if it cannot create a new subsock
    void *subsockFindOrCreate(const string& channel)
    {
        auto it = subsocks.find(channel);
        if (it != subsocks.end())
            return it->second;
        void *sock = zmq_socket(ctx, ZMQ_SUB);
        if (sock == nullptr) {
            ZCM_DEBUG("failed to create subsock: %s", zmq_strerror(errno));
            return nullptr;
        }
        string address = getAddress(channel);
        int rc;
        rc = zmq_connect(sock, address.c_str());
        if (rc == -1) {
            ZCM_DEBUG("failed to connect subsock: %s", zmq_strerror(errno));
            return nullptr;
        }
        rc = zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
        if (rc == -1) {
            ZCM_DEBUG("failed to setsockopt on subsock: %s", zmq_strerror(errno));
            return nullptr;
        }
        subsocks.emplace(channel, sock);
        return sock;
    }

    /********************** METHODS **********************/
    size_t getMtu()
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
        if (sock == nullptr)
            return ZCM_ECONNECT;
        int rc = zmq_send(sock, msg.buf, msg.len, 0);
        if (rc == (int)msg.len)
            return ZCM_EOK;
        assert(rc == -1);
        ZCM_DEBUG("zmq_send failed with: %s", zmq_strerror(errno));
        return ZCM_EUNKNOWN;
    }

    int recvmsgEnable(const char *channel, bool enable)
    {
        // Mutex used to protect 'subsocks' while allowing
        // recvmsgEnable() and recvmsg() to be called
        // concurrently
        unique_lock<mutex> lk(mut);

        assert(enable && "Disabling is not supported");
        if (channel == NULL) {
            recvAllChannels = true;
            return ZCM_EOK;
        }
        void *sock = subsockFindOrCreate(channel);
        if (sock == nullptr)
            return ZCM_ECONNECT;
        return ZCM_EOK;
    }

    void scanForNewChannels()
    {
        const char *prefix = NAME_PREFIX;
        size_t prefixLen = strlen(NAME_PREFIX);

        DIR *d;
        dirent *ent;

        if (!(d=opendir("/tmp/")))
            return;

        while ((ent=readdir(d)) != nullptr) {
            if (strncmp(ent->d_name, prefix, prefixLen) == 0) {
                string channel(ent->d_name + prefixLen);
                void *sock = subsockFindOrCreate(channel);
                if (sock == nullptr) {
                    ZCM_DEBUG("failed to open subsock in scanForNewChannels()");
                }
            }
        }

        closedir(d);
    }

    int recvmsg(zcm_msg_t *msg, int timeout)
    {
        // Build up a list of poll items
        vector<zmq_pollitem_t> pitems;
        vector<string> pchannels;
        {
            // Mutex used to protect 'subsocks' while allowing
            // recvmsgEnable() and recvmsg() to be called
            // concurrently
            unique_lock<mutex> lk(mut);

            if (recvAllChannels)
                scanForNewChannels();

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
        assert(rc != -1);
        if (rc >= 0) {
            for (size_t i = 0; i < pitems.size(); i++) {
                auto& p = pitems[i];
                if (p.revents != 0) {
                    int rc = zmq_recv(p.socket, recvmsgBuffer, MTU, 0);
                    if (rc == -1) {
                        ZCM_DEBUG("zmq_recv failed with: %s", zmq_strerror(errno));
                        assert(0 && "unexpected codepath");
                    }
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

    static size_t _getMtu(zcm_trans_t *zt)
    { return cast(zt)->getMtu(); }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static int _recvmsgEnable(zcm_trans_t *zt, const char *channel, bool enable)
    { return cast(zt)->recvmsgEnable(channel, enable); }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
    { return cast(zt)->recvmsg(msg, timeout); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    &ZCM_TRANS_CLASSNAME::_destroy,
};

extern "C" zcm_trans_t *zcm_trans_ipc_create()
{
    return new ZCM_TRANS_CLASSNAME();
}
