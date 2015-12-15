#include "zcm/server.h"
#include "zcm/url.h"
#include "zcm/util/debug.h"
#include "util/StringUtil.hpp"
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
using namespace std;

extern "C" zcm_trans_t *HAX_create_tcp_from_sock(int sock);

static bool asint(const string& s, int& v)
{
    if (s.size() == 1 && s[0] == '0') {
        v = 0;
        return true;
    }

    v = strtol(s.c_str(), NULL, 10);
    return v != 0;
}

class zcm_server_t
{
public:
    bool init(const char *url)
    {
        auto *u = zcm_url_create(url);
        string protocol = zcm_url_protocol(u);
        string address = zcm_url_address(u);
        zcm_url_destroy(u);

        if (protocol != "tcp") {
            ZCM_DEBUG("ZCM Server: only supports tcp protocol currently");
            return false;
        }

        auto parts = StringUtil::split(address, ':');
        if (parts.size() != 2) {
            ZCM_DEBUG("ZCM Server: expected an address of the form 'host:port'");
            return false;
        }

        // XXX verify the hostname is sane

        int port;
        if (!asint(parts[1], port)) {
            ZCM_DEBUG("ZCM Server: invalid value in the 'port' field: %s", parts[1].c_str());
            return false;
        }

        return initFromPort(port);
    }

    bool initFromPort(int port)
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ == -1)
            return false;

        int optval = 1;
        setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        //Prepare the sockaddr_in structure
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(port);

        if (::bind(sock_, (struct sockaddr *)&server, sizeof(server)) < 0)
            return false;

        if (::listen(sock_, 3) < 0) {
            return false;
        }

        return true;
    }

    ~zcm_server_t()
    {
        if (sock_ != -1)
            ::close(sock_);
    }

    zcm_t *accept(long timeout)
    {
        if (sock_ == -1)
            return NULL;

        socklen_t len;
        struct sockaddr client;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock_, &fds);

        struct timeval tm = {
            timeout / 1000,            /* seconds */
            (timeout % 1000) * 1000    /* micros */
        };

        int status = ::select(sock_ + 1, &fds, 0, 0, &tm);

        if (status == -1 || status == 0) {
            // timeout
            return NULL;
        } else if (FD_ISSET(sock_, &fds)) {
            // a connection is available
            int newsock = ::accept(sock_, &client, &len);
            if(newsock == -1) {
                perror("accept");
                int ret = ::fcntl(sock_, F_GETFD);
                ZCM_DEBUG("sock_ = %d, ret = %d", sock_, ret);
                return NULL;
            } else {
                ZCM_DEBUG("accept returned!");
                return makeZCMFromSocket(newsock);
            }
        } else {
            perror("zcm_server_accept -- select:");
            return NULL;
        }
    }

    zcm_t *makeZCMFromSocket(int newsock)
    {
        ZCM_DEBUG("Making socket!");
        auto *trans = HAX_create_tcp_from_sock(newsock);
        return zcm_create_trans(trans);
    }

private:
    int sock_ = -1;
};

/////////////// C Interface Functions ////////////////
extern "C" {

zcm_server_t *zcm_server_create(const char *url)
{
    auto *svr = new zcm_server_t();
    if (svr->init(url))
        return svr;

    // It failed! Clean it up!
    delete svr;
    return NULL;
}

zcm_t *zcm_server_accept(zcm_server_t *svr, long timeout)
{
    return svr->accept(timeout);
}

void zcm_server_destroy(zcm_server_t *svr)
{
    delete svr;
}

}
