#pragma once
#include "udp.hpp"
#include "buffers.hpp"

class UDPAddress
{
  public:
    UDPAddress(const string& ip, u16 port)
    {
        this->ip = ip;
        this->port = port;

        memset(&this->addr, 0, sizeof(this->addr));
        this->addr.sin_family = AF_INET;
        inet_aton(ip.c_str(), &this->addr.sin_addr);
        this->addr.sin_port = htons(port);
    }

    const string& getIP() const { return ip; }
    u16 getPort() const { return port; }
    struct sockaddr* getAddrPtr() const { return (struct sockaddr*)&addr; }
    size_t getAddrSize() const { return sizeof(addr); }

  private:
    string ip;
    u16 port;
    struct sockaddr_in addr;
};

class UDPSocket
{
  public:
    UDPSocket();
    ~UDPSocket();
    bool isOpen();
    void close();

    bool init();
    bool joinMulticastGroup(struct in_addr multiaddr);
    bool setTTL(u8 ttl, bool multicast);
    bool bindPort(u16 port);
    bool setReuseAddr();
    bool setReusePort();
    bool enablePacketTimestamp();
    bool enableMulticastLoopback();
    bool setDestination(const string& ip, u16 port);

    size_t getRecvBufSize();
    size_t getSendBufSize();

    // Returns true when there is a packet available for receiving
    bool waitUntilData(unsigned timeout);
    int recvPacket(Packet *pkt);

    ssize_t sendBuffers(const UDPAddress& dest, const char *a, size_t alen);
    ssize_t sendBuffers(const UDPAddress& dest, const char *a, size_t alen,
                            const char *b, size_t blen);
    ssize_t sendBuffers(const UDPAddress& dest, const char *a, size_t alen,
                        const char *b, size_t blen, const char *c, size_t clen);

    static bool checkConnection(const string& ip, u16 port);
    void checkAndWarnAboutSmallBuffer(size_t datalen, size_t kbufsize);

    static UDPSocket createSendSocket(struct in_addr addr, u8 ttl, bool multicast);
    static UDPSocket createRecvSocket(struct in_addr addr, u16 port, bool multicast);

  private:
    SOCKET fd = -1;
    bool warnedAboutSmallBuffer = false;

  private:
    // Disallow copies
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;

  public:
    // Allow moves
    UDPSocket(UDPSocket&& other) { std::swap(this->fd, other.fd); }
    UDPSocket& operator=(UDPSocket&& other) { std::swap(this->fd, other.fd); return *this; }
};
