#ifndef _RAW_ETH_
#define _RAW_ETH_

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <cstring>
#include <cstdlib>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <string>
#include <array>
#include <cctype>

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
        throw Exception("cannot get index of interface");
    }

    // get mac
    struct ifreq if_mac;
    memset(&if_mac, 0, sizeof(struct ifreq));
    strncpy(if_mac.ifr_name, if_name, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
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

}

#endif // _RAW_ETH_
