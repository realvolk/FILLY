#include <napi.h>
#include "core/client.h"

Napi::Value Connect(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    std::string path = info[0].As<Napi::String>().Utf8Value();
    FillyClient* c = filly_client_connect(path.c_str());
    if (!c) {
        Napi::Error::New(env, "Cannot connect").ThrowAsJavaScriptException();
        return env.Null();
    }
    return Napi::External<FillyClient>::New(env, c);
}

Napi::Value Send(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    FillyClient* c = info[0].As<Napi::External<FillyClient>>().Data();
    std::string json = info[1].As<Napi::String>().Utf8Value();
    filly_client_send_request(c, json.c_str());
    return env.Undefined();
}

Napi::Value Close(const Napi::CallbackInfo& info) {
    FillyClient* c = info[0].As<Napi::External<FillyClient>>().Data();
    filly_client_disconnect(c);
    return info.Env().Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("connect", Napi::Function::New(env, Connect));
    exports.Set("send", Napi::Function::New(env, Send));
    exports.Set("close", Napi::Function::New(env, Close));
    return exports;
}
NODE_API_MODULE(filly, Init)