#ifndef _RAW_ETH_
#define _RAW_ETH_

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <cstring>
#include <cstdlib>
#include <sys/ioctl.h>
#include <libexplain/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <string>
#include <array>
#include <cctype>
#include <iostream>

#include <nan.h>
#include <node.h>

namespace node_packet {

struct Exception : public std::exception
{
    std::string _what;

    Exception(std::string what)
        : _what(what)
    { }

    const char *
    what() const noexcept
    { return _what.c_str(); }
};

#define hex2num(c)  \
    (std::isdigit(c) ? (c - '0') : (std::tolower(c) - 'a' + 10))

static inline void
parse_mac(const char *literal, char *buffer)
{
    *buffer++ = (hex2num(literal[0]) << 16) + hex2num(literal[1]);
    literal += 3;
    *buffer++ = (hex2num(literal[0]) << 16) + hex2num(literal[1]);
    literal += 3;
    *buffer++ = (hex2num(literal[0]) << 16) + hex2num(literal[1]);
    literal += 3;
    *buffer++ = (hex2num(literal[0]) << 16) + hex2num(literal[1]);
    literal += 3;
    *buffer++ = (hex2num(literal[0]) << 16) + hex2num(literal[1]);
    literal += 3;
    *buffer++ = (hex2num(literal[0]) << 16) + hex2num(literal[1]);
    literal += 3;
}

static inline int
send_raw_packet(const char *if_name, const char *dst_mac, const char *content, size_t length)
{
    // open socket
    int sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
    if (sockfd == -1) {
        throw Exception("cannot open socket");
    }

    // get index
    struct ifreq if_idx;
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, if_name, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
        std::cout << explain_ioctl(sockfd, SIOCGIFINDEX, &if_idx) << std::endl;
        throw Exception("cannot get index of interface");
    }

    // get mac
    struct ifreq if_mac;
    memset(&if_mac, 0, sizeof(struct ifreq));
    strncpy(if_mac.ifr_name, if_name, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
        std::cout << explain_ioctl(sockfd, SIOCGIFINDEX, &if_idx) << std::endl;
        throw Exception("cannot get address of interface");
    }

    // construct Ethernet header
    uint8_t buffer[length + 16];
    struct ether_header *header = reinterpret_cast<struct ether_header *>(buffer);

    memcpy(header->ether_shost, if_mac.ifr_hwaddr.sa_data, 6);
    memcpy(header->ether_dhost, dst_mac, 6);
    header->ether_type = htons(ETH_P_IP);
    memcpy(buffer + sizeof(ether_header), content, length);

    // config address
    sockaddr_ll address;
    address.sll_ifindex = if_idx.ifr_ifindex;
    address.sll_halen = ETH_ALEN;
    memcpy(address.sll_addr, dst_mac, 6);

    // send
    return sendto(
        sockfd,
        buffer,
        length + 16,
        0,
        reinterpret_cast<struct sockaddr*>(&address),
        sizeof(struct sockaddr_ll)
    );
}

class Listener : public Nan::AsyncProgressWorker
{
    int sockfd;

protected:
    virtual void
    HandleOKCallback()
    { }

    virtual void
    HandleErrorCallback()
    { }

    using Nan::AsyncProgressWorker::ExecutionProgress;

public:
    Listener(const char *if_name, Nan::Callback *callback)
        : Nan::AsyncProgressWorker(callback)
    {
        sockfd = socket(PF_PACKET, SOCK_RAW, htons(0x0800));
        if (sockfd == -1) {
            throw Exception("cannot open socket");
        }

        struct ifreq ifopts;
        strncpy(ifopts.ifr_name, if_name, IFNAMSIZ - 1);
        ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
        ifopts.ifr_flags |= IFF_PROMISC;
        ioctl(sockfd, SIOCGIFFLAGS, &ifopts);

        int sockopt;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) == -1) {
            throw Exception("cannot set sockopt");
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, if_name, IFNAMSIZ - 1) == -1) {
            throw Exception("cannot bind to device");
        }
    }

    virtual void
    Execute(const ExecutionProgress &progress)
    {
        std::cout << "execute" << std::endl;
        char buffer[1600];
        size_t numbytes;
        while (true) {
            numbytes = recvfrom(sockfd, buffer, 1600, 0, NULL, NULL);
            progress.Send(buffer, numbytes);
        }
    }

    virtual void
    HandleProgressCallback(const char *data, size_t size)
    {
        Nan::HandleScope scope;
        v8::Local<v8::Value> argv[] = { Nan::CopyBuffer(data, size).ToLocalChecked() };
        callback->Call(1, argv);
    }

    virtual void
    Destroy()
    { }

    virtual void
    WorkComplete()
    { }
};

}

#endif // _RAW_ETH_
