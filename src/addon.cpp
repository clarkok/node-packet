#include <node.h>
#include <nan.h>

#include "raw_eth.hpp"

namespace node_packet {

using v8::FunctionCallbackInfo;
using v8::Value;
using v8::Isolate;
using v8::String;
using v8::Local;
using v8::Object;
using v8::Number;

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

    char dst_mac[6];
    char buffer[18];
    if (Nan::DecodeWrite(buffer, 17, args[1], Nan::Encoding::UTF8) != 17) {
        isolate->ThrowException(v8::Exception::TypeError(
            String::NewFromUtf8(isolate, "invalid mac")));
        return;
    }
    parse_mac(buffer, dst_mac);

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
init(Local<Object> exports)
{
    NODE_SET_METHOD(exports, "hello", hello);
    NODE_SET_METHOD(exports, "send", send);
}

NODE_MODULE(addon, init)

}
