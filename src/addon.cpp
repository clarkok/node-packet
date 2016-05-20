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
#include <node.h>
#include <nan.h>
#include <cstdio>

namespace node_packet {

using v8::FunctionCallbackInfo;
using v8::Value;
using v8::Isolate;
using v8::String;
using v8::Local;
using v8::Object;
using v8::Number;

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
    std::printf("%.17s ->", literal);

    buffer[0] = (hex2num(literal[0]) << 4) + hex2num(literal[1]);
    literal += 3;
    buffer[1] = (hex2num(literal[0]) << 4) + hex2num(literal[1]);
    literal += 3;
    buffer[2] = (hex2num(literal[0]) << 4) + hex2num(literal[1]);
    literal += 3;
    buffer[3] = (hex2num(literal[0]) << 4) + hex2num(literal[1]);
    literal += 3;
    buffer[4] = (hex2num(literal[0]) << 4) + hex2num(literal[1]);
    literal += 3;
    buffer[5] = (hex2num(literal[0]) << 4) + hex2num(literal[1]);

    for (int i = 0; i < 6; ++i) {
        std::printf(" %x", buffer[i]);
    }
    std::printf("\n");
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
        sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
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

class AsyncPacketTranreceiver : public Nan::AsyncWorker
{
protected:

};

void
hello(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = args.GetIsolate();
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "world"));
}

void
send(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = args.GetIsolate();

    if (args.Length() < 3) {
        isolate->ThrowException(v8::Exception::TypeError(
            String::NewFromUtf8(isolate, "Wrong number of arguments, need 3")));
        return;
    }

    if (!args[0]->IsString() || !args[1]->IsString()) {
        isolate->ThrowException(v8::Exception::TypeError(
            String::NewFromUtf8(isolate, "Wrong argument type")));
        return;
    }

    char if_name[IFNAMSIZ];
    Nan::DecodeWrite(if_name, IFNAMSIZ - 1, args[0], Nan::Encoding::UTF8);
    if_name[args[0]->ToString(isolate)->Length()] = '\0';

    char mac_literal[18];
    Nan::DecodeWrite(mac_literal, 17, args[1], Nan::Encoding::UTF8);
    mac_literal[args[1]->ToString(isolate)->Length()] = '\0';

    char dst_mac[6];
    parse_mac(mac_literal, dst_mac);

    try {
        auto result = send_raw_packet(if_name, dst_mac, node::Buffer::Data(args[2]), node::Buffer::Length(args[2]));
        args.GetReturnValue().Set(Number::New(isolate, result));
    }
    catch (const Exception &e) {
        isolate->ThrowException(v8::Exception::Error(
            String::NewFromUtf8(isolate, e.what())));
        return;
    }
}

void
listen(const FunctionCallbackInfo<Value> &args)
{
    Isolate *isolate = args.GetIsolate();

    if (args.Length() < 2) {
        isolate->ThrowException(v8::Exception::TypeError(
            String::NewFromUtf8(isolate, "Wrong number of arguments, need 2")));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(v8::Exception::TypeError(
            String::NewFromUtf8(isolate, "Wrong argument type")));
        return;
    }

    char if_name[IFNAMSIZ];
    Nan::DecodeWrite(if_name, IFNAMSIZ - 1, args[0], Nan::Encoding::UTF8);
    if_name[args[0]->ToString(isolate)->Length()] = '\0';

    try {
        Nan::AsyncQueueWorker(new Listener(if_name, new Nan::Callback(args[1].As<v8::Function>())));
        args.GetReturnValue().Set(v8::Undefined(isolate));
    }
    catch (const Exception &e) {
        isolate->ThrowException(v8::Exception::Error(
            String::NewFromUtf8(isolate, e.what())));
        return;
    }
}

void
init(Local<Object> exports)
{
    NODE_SET_METHOD(exports, "hello", hello);
    NODE_SET_METHOD(exports, "send", send);
    NODE_SET_METHOD(exports, "listen", listen);
}

NODE_MODULE(addon, init)

}
