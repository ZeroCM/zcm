#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/transport_register.hpp"
#include "zcm/server.h"
#include "zcm/util/debug.h"
#include "util/StringUtil.hpp"
#include <cassert>
#include <cstring>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
using std::string;

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportTCP
#define MTU (1<<20)

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

static bool asint(const string& s, int& v)
{
    if (s.size() == 1 && s[0] == '0') {
        v = 0;
        return true;
    }

    v = strtol(s.c_str(), NULL, 10);
    return v != 0;
}

class TCPComm
{
public:
    TCPComm(int sock)
    : sock_(sock)
    {
    }

    ~TCPComm()
    {
        close();
    }

    bool good()
    {
        return sock_ != -1;
    }

    void close()
    {
        if (sock_ != -1) {
            ::close(sock_);
            sock_ = -1;
        }
    }

    /* Blocks until all the requested data is sent (accepted by kernel)
       Returns true if data was successfully sent in its entirety
       Returns false if an unrecoverable error occurred and the data wasn't send
    */
    bool write(const char *data, size_t len)
    {
        if (sock_ == -1)
            return false;

        size_t n = 0;
        while (n < len) {
            ssize_t ret = ::send(sock_, data+n, len-n, 0);
            if (ret >= 0) {
                // all is good!
                n += (size_t)ret;
                continue;
            }

            // Opps an error occurred
            // Usually bad, but if we were interrupted by a signal it's okay.
            if (errno == EINTR) {
                continue;
            }

            // Nope, definately bad.
            ZCM_DEBUG("Error on tcp socket write. Closing the socket.");
            this->close();
            return false;
        }

        assert(n == len);
        return true;
        // TODO write me
    }

    /* Blocks until all the requested data is read (i.e. 'len' bytes are read)
       Returns true if the entire size requested was read
       Returns false if an unrecoverable error occurred and the data wasn't read
    */
    bool read(char *data, size_t len)
    {
        // XXX read should use a buffer to prevent so many syscalls
        if (sock_ == -1)
            return false;


        size_t n = 0;
        while (n < len) {
            ssize_t ret = ::recv(sock_, data+n, len-n, 0);
            if (ret >= 0) {
                // all is good!
                n += (size_t)ret;
                continue;
            }

            // Opps an error occurred
            // Usually bad, but if we were interrupted by a signal it's okay.
            if (errno == EINTR) {
                continue;
            }

            // Nope, definately bad.
            ZCM_DEBUG("Error on tcp socket read. Closing the socket.");
            perror("Foobar");
            this->close();
            return false;
        }

        assert(n == len);
        return true;
    }

    template<class T>
    bool write(const T& val)
    {
        return write((const char*)&val, sizeof(val));
    }

    template<class T>
    bool read(const T& val)
    {
        return read((char*)&val, sizeof(val));
    }

private:
    int sock_ = -1;
};

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    TCPComm tcp;

    // Preallocated memory for recv
    char recvChannelMem[ZCM_CHANNEL_MAXLEN+1];
    char recvDataMem[MTU];

    ZCM_TRANS_CLASSNAME(int sock)
    : tcp(sock)
    {
        trans_type = ZCM_BLOCKING;
        vtbl = &methods;
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        ZCM_DEBUG("TCP shutting down!");
    }

    bool good()
    {
        return tcp.good();
    }

    /********************** METHODS **********************/
    size_t getMtu()
    {
        return MTU;
    }

    // Very simple protocol:
    //    32-bit channel len
    //    32-bit data len
    //    channel bytes
    //    data bytes

    int sendmsg(zcm_msg_t msg)
    {
        ZCM_DEBUG("SENDING!\n");

        u32 clen = (u32)strlen(msg.channel);
        u32 dlen = (u32)msg.len;

        if (clen > ZCM_CHANNEL_MAXLEN)
            return ZCM_EINVALID;
        if (dlen > MTU)
            return ZCM_EINVALID;

        int ret = 1;
        ret &= tcp.write(clen);
        ret &= tcp.write(dlen);
        ret &= tcp.write(msg.channel, clen);
        ret &= tcp.write(msg.buf, dlen);

        ZCM_DEBUG("SENDING DONE!\n");
        return ret ? ZCM_EOK : ZCM_ECONNECT;
    }

    int recvmsgEnable(const char *channel, bool enable)
    {
        return ZCM_EOK;
    }

    int recvmsg(zcm_msg_t *msg, int timeout)
    {
        u32 clen, dlen;
        int ret;

        ret = 1;
        ret &= tcp.read(clen);
        ret &= tcp.read(dlen);

        if (!ret || clen > ZCM_CHANNEL_MAXLEN || dlen > MTU)
            return ZCM_EAGAIN;

        ret = 1;
        ret &= tcp.read(recvChannelMem, clen);
        ret &= tcp.read(recvDataMem, dlen);
        if (!ret)
            return ZCM_EAGAIN;

        msg->channel = recvChannelMem;
        msg->buf = recvDataMem;
        msg->len = dlen;

        return ZCM_EOK;
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

    static const TransportRegister reg;
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    NULL, // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static zcm_trans_t *create(zcm_url_t *url)
{
    string address = zcm_url_address(url);
    auto parts = StringUtil::split(address, ':');
    if (parts.size() != 2) {
        ZCM_DEBUG("tcp create: expected an address of the form 'host:port'");
        return NULL;
    }

    string host = parts[0];

    int port;
    if (!asint(parts[1], port)) {
        ZCM_DEBUG("tcp create: invalid value in the 'port' field: %s", parts[1].c_str());
        return NULL;
    }

    // create a connecting socket
    int sock;
    if ((sock=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        ZCM_DEBUG("tcp create: failed to create socket");
        return NULL;
    }

    // Resolve the hostname
    struct hostent *he = gethostbyname(host.c_str());
    if (he == nullptr) {
        ZCM_DEBUG("tcp create: failed resolve hostname: %s", host.c_str());
        ::close(sock);
        return NULL;
    }

    struct sockaddr_in server;
    memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (::connect(sock, (struct sockaddr *)&server , sizeof(server)) < 0) {
        ZCM_DEBUG("tcp create: failed connect");
        ::close(sock);
        return NULL;
    }

    auto *trans = new ZCM_TRANS_CLASSNAME(sock);
    if (trans->good())
        return trans;

    delete trans;
    return nullptr;
}

// Register this transport with ZCM
const TransportRegister ZCM_TRANS_CLASSNAME::reg(
    "tcp", "Transfer data via a tcp connection (e.g. 'tcp://some.host.name:12345')",
    create);

class Server : public zcm_server_t
{
public:
    Server()
    {
        vtbl = &methods;
    }

    bool init(zcm_url_t *u)
    {
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

    ~Server()
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
        auto *trans = new ZCM_TRANS_CLASSNAME(newsock);
        return zcm_create_trans(trans);
    }

private:
    int sock_ = -1;

public:
    /********************** STATICS **********************/
    static zcm_server_methods_t methods;
    static Server *cast(zcm_server_t *svr)
    {
        assert(svr->vtbl == &methods);
        return (Server*) svr;
    }

    static zcm_t *_accept(zcm_server_t *svr, int timeout)
    { return cast(svr)->accept(timeout); }

    static void _destroy(zcm_server_t *svr)
    { delete cast(svr); }
};

zcm_server_methods_t Server::methods = {
    &Server::_accept,
    &Server::_destroy,
};

extern "C" zcm_server_t *transport_tcp_server_create(zcm_url_t *url)
{
    Server *svr = new Server();
    if (svr->init(url))
        return svr;

    // failed!
    delete svr;
    return nullptr;
}
