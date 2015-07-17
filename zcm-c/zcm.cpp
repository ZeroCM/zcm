#include "zcm.h"
#include <zmq.h>

#include <unistd.h>
#include <dirent.h>

#include <cstring>
#include <cassert>
#include <cctype>
#include <cstdio>

#include <vector>
using std::vector;

#include <unordered_map>
using std::unordered_map;

#include <string>
using std::string;

#include <mutex>
#include <thread>
#include <iostream>

#define METADATA_PUB_ADDR "ipc:///tmp/zcm-metadata-pub"
#define METADATA_SUB_ADDR "ipc:///tmp/zcm-metadata-sub"
#define ZMQ_IO_THREADS 1

struct Sub
{
    zcm_callback_t *cb;
    void *usr;
    void *sock;
};

struct SubRegex
{
    string channelRegex;
    zcm_callback_t *cb;
    void *usr;
};

struct Pub
{
    void *sock;
};

struct zcm_t
{
    // The mutex protects all data below it
    std::mutex mut;
    static constexpr int RECVBUFSZ = 1 << 20;
    char recvbuf[RECVBUFSZ];

    void *ctx;
    unordered_map<string, Sub> subs;
    unordered_map<string, Pub> pubs;

    vector<SubRegex> subRegex;

    zcm_t()
    {
        ctx = zmq_init(ZMQ_IO_THREADS);
    }

    string getIPCAddress(const string& channel)
    {
        return "ipc:///tmp/zmq-channel-"+channel;
    }

    Sub *findSub(const string& channel)
    {
        auto it = subs.find(channel);
        if (it != subs.end())
            return &it->second;
        else
            return nullptr;
    }

    Pub *findPub(const string& channel)
    {
        auto it = pubs.find(channel);
        if (it != pubs.end())
            return &it->second;
        else
            return nullptr;
    }

    int publish(const string& channel, const char *data, size_t len)
    {
        std::unique_lock<std::mutex> lk(mut);

        Pub *pub = findPub(channel);
        if (!pub) {
            void *sock = zmq_socket(ctx, ZMQ_PUB);
            string address = getIPCAddress(channel);
            zmq_bind(sock, address.c_str());
            pubs.emplace(channel, Pub{sock});
            pub = findPub(channel);
            assert(pub);
        }

        int rc = zmq_send(pub->sock, data, len, 0);
        assert(rc == (int)len);
        return 0;
    }

    void searchForRegexSubs()
    {
        std::unique_lock<std::mutex> lk(mut);

        // TODO: actually implement regex, or remove it
        if (subRegex.size() == 0)
            return;
        for (auto& r : subRegex) {
            if (r.channelRegex != ".*") {
                static bool warned = false;
                if (!warned) {
                    fprintf(stderr, "Er: only .* (aka subscribe-all) is implemented\n");
                    warned = true;
                }
            }
        }
        SubRegex& srex = subRegex[0];

        const char *prefix = "zmq-channel-";
        size_t prefixLen = strlen(prefix);

        DIR *d;
        dirent *ent;

        if (!(d=opendir("/tmp/")))
            return;

        while ((ent=readdir(d)) != nullptr) {
            if (strncmp(ent->d_name, prefix, prefixLen) == 0) {
                string channel(ent->d_name + prefixLen);
                if (!findSub(channel))
                    subOneInternal(channel, srex.cb, srex.usr);
            }
        }

        closedir(d);
    }

    void recvMsgFromSock(const string& channel, void *sock)
    {
        int rc = zmq_recv(sock, recvbuf, RECVBUFSZ, 0);
        assert(0 < rc && rc < RECVBUFSZ);

        // Dispatch
        {
            std::unique_lock<std::mutex> lk(mut);
            if (Sub *sub = findSub(channel)) {
                zcm_recv_buf_t rbuf;
                rbuf.data = recvbuf;
                rbuf.len = rc;
                rbuf.utime = 0;
                rbuf.zcm = this;
                sub->cb(&rbuf, channel.c_str(), sub->usr);
            }
        }
    }

    int handleTimeout(long timeout)
    {
        searchForRegexSubs();

        // Build up a list of poll items
        vector<zmq_pollitem_t> pitems;
        vector<string> pchannels;
        {
            std::unique_lock<std::mutex> lk(mut);
            pitems.resize(subs.size());
            int i = 0;
            for (auto& elt : subs) {
                auto& channel = elt.first;
                auto& sub = elt.second;
                auto *p = &pitems[i];
                memset(p, 0, sizeof(*p));
                p->socket = sub.sock;
                p->events = ZMQ_POLLIN;
                pchannels.emplace_back(channel);
                i++;
            }
        }

        int rc = zmq_poll(pitems.data(), pitems.size(), timeout);
        if (rc >= 0) {
            for (size_t i = 0; i < pitems.size(); i++) {
                auto& p = pitems[i];
                if (p.revents != 0)
                    recvMsgFromSock(pchannels[i], p.socket);
            }
        }

        return rc;
    }

    // TODO: can we get rid of this and go full blocking?
    int handle()
    {
        while (true) {
            int rc = handleTimeout(100);
            if (rc >= 0)
                break;
        }
        return 0;
    }

    static bool isRegexChannel(const string& channel)
    {
        // These chars are considered regex
        auto isRegexChar = [](char c) {
            return c == '(' || c == ')' || c == '|' ||
                   c == '.' || c == '*' || c == '+';
        };

        for (auto& c : channel)
            if (isRegexChar(c))
                return true;

        return false;
    }

    void subOneInternal(const string& channel, zcm_callback_t *cb, void *usr)
    {
        assert(!findSub(channel));

        void *sock = zmq_socket(ctx, ZMQ_SUB);
        string address = getIPCAddress(channel);
        zmq_connect(sock, address.c_str());
        zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);

        subs.emplace(channel, Sub{cb, usr, sock});
    }

    int subscribe(const string& channel, zcm_callback_t *cb, void *usr)
    {
        std::unique_lock<std::mutex> lk(mut);

        if (isRegexChannel(channel)) {
            subRegex.emplace_back(SubRegex{channel, cb, usr});
        } else {
            subOneInternal(channel, cb, usr);
        }

        return 0;
    }
};

/////////////// C Interface Functions ////////////////

zcm_t *zcm_create(void)
{
    return new zcm_t();
}

void zcm_destroy(zcm_t *zcm)
{
    if (zcm) delete zcm;
}

int zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len)
{
    return zcm->publish(channel, data, len);
}

int zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
    return zcm->subscribe(channel, cb, usr);
}

int zcm_handle(zcm_t *zcm)
{
    return zcm->handle();
}

int zcm_handle_timeout(zcm_t *zcm, uint ms)
{
    return zcm->handleTimeout((long)ms);
}
