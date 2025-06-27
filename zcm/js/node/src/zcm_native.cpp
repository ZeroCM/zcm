#include <napi.h>
#include <zcm/zcm.h>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <mutex>

class ZcmWrapper : public Napi::ObjectWrap<ZcmWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    ZcmWrapper(const Napi::CallbackInfo& info);
    ~ZcmWrapper();

private:
    static Napi::FunctionReference constructor;

    // ZCM instance
    zcm_t* zcm_;

    // Subscription management
    struct SubscriptionInfo {
        uint32_t id;
        zcm_sub_t* sub;
        Napi::ThreadSafeFunction tsfn;
        std::string channel;
    };

    std::map<uint32_t, SubscriptionInfo> subscriptions_;
    uint32_t next_sub_id_;
    std::mutex sub_mutex_;

    // Methods
    Napi::Value Publish(const Napi::CallbackInfo& info);
    Napi::Value Subscribe(const Napi::CallbackInfo& info);
    Napi::Value Unsubscribe(const Napi::CallbackInfo& info);
    Napi::Value Start(const Napi::CallbackInfo& info);
    Napi::Value Stop(const Napi::CallbackInfo& info);
    Napi::Value Flush(const Napi::CallbackInfo& info);
    Napi::Value Pause(const Napi::CallbackInfo& info);
    Napi::Value Resume(const Napi::CallbackInfo& info);
    Napi::Value SetQueueSize(const Napi::CallbackInfo& info);
    Napi::Value WriteTopology(const Napi::CallbackInfo& info);
    Napi::Value Destroy(const Napi::CallbackInfo& info);

    // Static callback for ZCM
    static void MessageHandler(const zcm_recv_buf_t* rbuf, const char* channel, void* usr);
};

// Static member initialization
Napi::FunctionReference ZcmWrapper::constructor;

// Structure for passing data to JavaScript callback
struct CallbackData {
    std::string channel;
    std::vector<uint8_t> data;
    uint64_t utime;
};

Napi::Object ZcmWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function func = DefineClass(env, "ZcmNative", {
        InstanceMethod("publish", &ZcmWrapper::Publish),
        InstanceMethod("subscribe", &ZcmWrapper::Subscribe),
        InstanceMethod("unsubscribe", &ZcmWrapper::Unsubscribe),
        InstanceMethod("start", &ZcmWrapper::Start),
        InstanceMethod("stop", &ZcmWrapper::Stop),
        InstanceMethod("flush", &ZcmWrapper::Flush),
        InstanceMethod("pause", &ZcmWrapper::Pause),
        InstanceMethod("resume", &ZcmWrapper::Resume),
        InstanceMethod("setQueueSize", &ZcmWrapper::SetQueueSize),
        InstanceMethod("writeTopology", &ZcmWrapper::WriteTopology),
        InstanceMethod("destroy", &ZcmWrapper::Destroy),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("ZcmNative", func);
    return exports;
}

ZcmWrapper::ZcmWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ZcmWrapper>(info) {
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);

    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Expected URL argument").ThrowAsJavaScriptException();
        return;
    }

    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "URL must be a string").ThrowAsJavaScriptException();
        return;
    }

    std::string url = info[0].As<Napi::String>().Utf8Value();
    zcm_ = zcm_create(url.c_str());

    if (!zcm_) {
        Napi::Error::New(env, "Failed to create ZCM instance").ThrowAsJavaScriptException();
        return;
    }

    next_sub_id_ = 0;
}

ZcmWrapper::~ZcmWrapper() {
    if (zcm_) {
        // Clean up subscriptions
        std::lock_guard<std::mutex> lock(sub_mutex_);
        for (auto& pair : subscriptions_) {
            zcm_try_unsubscribe(zcm_, pair.second.sub);
            pair.second.tsfn.Release();
        }
        subscriptions_.clear();

        zcm_destroy(zcm_);
        zcm_ = nullptr;
    }
}

Napi::Value ZcmWrapper::Publish(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2) {
        Napi::TypeError::New(env, "Expected channel and data arguments").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "Channel must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[1].IsBuffer()) {
        Napi::TypeError::New(env, "Data must be a buffer").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string channel = info[0].As<Napi::String>().Utf8Value();
    Napi::Buffer<uint8_t> data = info[1].As<Napi::Buffer<uint8_t>>();

    int ret = zcm_publish(zcm_, channel.c_str(), data.Data(), data.Length());

    return Napi::Number::New(env, ret);
}

void ZcmWrapper::MessageHandler(const zcm_recv_buf_t* rbuf, const char* channel, void* usr) {
    SubscriptionInfo* subInfo = static_cast<SubscriptionInfo*>(usr);

    // Create callback data
    CallbackData* cbData = new CallbackData();
    cbData->channel = std::string(channel);
    cbData->data.assign(rbuf->data, rbuf->data + rbuf->data_size);
    cbData->utime = rbuf->recv_utime;

    // Call JavaScript callback
    subInfo->tsfn.NonBlockingCall(cbData, [](Napi::Env env, Napi::Function jsCallback, CallbackData* data) {
        // Create JavaScript objects
        Napi::String jsChannel = Napi::String::New(env, data->channel);
        Napi::Buffer<uint8_t> jsData = Napi::Buffer<uint8_t>::Copy(env, data->data.data(), data->data.size());

        // Call the JavaScript callback
        jsCallback.Call({jsChannel, jsData});

        // Clean up
        delete data;
    });
}

Napi::Value ZcmWrapper::Subscribe(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2) {
        Napi::TypeError::New(env, "Expected channel and callback arguments").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "Channel must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[1].IsFunction()) {
        Napi::TypeError::New(env, "Callback must be a function").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string channel = info[0].As<Napi::String>().Utf8Value();
    Napi::Function callback = info[1].As<Napi::Function>();

    // Create subscription info
    std::lock_guard<std::mutex> lock(sub_mutex_);
    uint32_t subId = next_sub_id_++;

    SubscriptionInfo subInfo;
    subInfo.id = subId;
    subInfo.channel = channel;

    // Create thread-safe function
    subInfo.tsfn = Napi::ThreadSafeFunction::New(
        env,
        callback,
        "ZCM Subscription",
        0,
        1,
        [this, subId](Napi::Env) {
            // Cleanup when thread-safe function is finalized
            std::lock_guard<std::mutex> lock(sub_mutex_);
            subscriptions_.erase(subId);
        }
    );

    // Store subscription info before calling zcm_try_subscribe
    subscriptions_[subId] = subInfo;

    // Subscribe to ZCM
    zcm_sub_t* sub = zcm_try_subscribe(zcm_, channel.c_str(), MessageHandler, &subscriptions_[subId]);

    if (!sub) {
        subscriptions_.erase(subId);
        Napi::Error::New(env, "Failed to subscribe to channel").ThrowAsJavaScriptException();
        return env.Null();
    }

    subscriptions_[subId].sub = sub;

    return Napi::Number::New(env, subId);
}

Napi::Value ZcmWrapper::Unsubscribe(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Expected subscription ID argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "Subscription ID must be a number").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint32_t subId = info[0].As<Napi::Number>().Uint32Value();

    std::lock_guard<std::mutex> lock(sub_mutex_);
    auto it = subscriptions_.find(subId);
    if (it == subscriptions_.end()) {
        return Napi::Number::New(env, ZCM_EINVALID);
    }

    int ret = zcm_try_unsubscribe(zcm_, it->second.sub);
    if (ret == ZCM_EOK) {
        it->second.tsfn.Release();
        subscriptions_.erase(it);
    }

    return Napi::Number::New(env, ret);
}

Napi::Value ZcmWrapper::Start(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    zcm_start(zcm_);
    return env.Undefined();
}

Napi::Value ZcmWrapper::Stop(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int ret = zcm_try_stop(zcm_);
    return Napi::Number::New(env, ret);
}

Napi::Value ZcmWrapper::Flush(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int ret = zcm_try_flush(zcm_);
    return Napi::Number::New(env, ret);
}

Napi::Value ZcmWrapper::Pause(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    zcm_pause(zcm_);
    return env.Undefined();
}

Napi::Value ZcmWrapper::Resume(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    zcm_resume(zcm_);
    return env.Undefined();
}

Napi::Value ZcmWrapper::SetQueueSize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Expected queue size argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "Queue size must be a number").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint32_t size = info[0].As<Napi::Number>().Uint32Value();
    int ret = zcm_try_set_queue_size(zcm_, size);

    return Napi::Number::New(env, ret);
}

Napi::Value ZcmWrapper::WriteTopology(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Expected filename argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "Filename must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string filename = info[0].As<Napi::String>().Utf8Value();
    int ret = zcm_write_topology(zcm_, filename.c_str());

    return Napi::Number::New(env, ret);
}

Napi::Value ZcmWrapper::Destroy(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (zcm_) {
        std::lock_guard<std::mutex> lock(sub_mutex_);
        for (auto& pair : subscriptions_) {
            zcm_try_unsubscribe(zcm_, pair.second.sub);
            pair.second.tsfn.Release();
        }
        subscriptions_.clear();

        zcm_destroy(zcm_);
        zcm_ = nullptr;
    }

    return env.Undefined();
}

// Module initialization
Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
    // Export ZCM return codes
    exports.Set("ZCM_EOK", Napi::Number::New(env, ZCM_EOK));
    exports.Set("ZCM_EINVALID", Napi::Number::New(env, ZCM_EINVALID));
    exports.Set("ZCM_EAGAIN", Napi::Number::New(env, ZCM_EAGAIN));
    exports.Set("ZCM_ECONNECT", Napi::Number::New(env, ZCM_ECONNECT));
    exports.Set("ZCM_EINTR", Napi::Number::New(env, ZCM_EINTR));
    exports.Set("ZCM_EUNKNOWN", Napi::Number::New(env, ZCM_EUNKNOWN));
    exports.Set("ZCM_EMEMORY", Napi::Number::New(env, ZCM_EMEMORY));
    exports.Set("ZCM_EUNIMPL", Napi::Number::New(env, ZCM_EUNIMPL));
    exports.Set("ZCM_NUM_RETURN_CODES", Napi::Number::New(env, ZCM_NUM_RETURN_CODES));

    // Export the ZCM wrapper class
    return ZcmWrapper::Init(env, exports);
}

NODE_API_MODULE(zcm_native, InitAll)
