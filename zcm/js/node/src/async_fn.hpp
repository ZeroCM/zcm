#pragma once

#include <napi.h>
#include <functional>
#include <string>
#include <tuple>
#include <vector>
#include <type_traits>

// Helper struct to convert types to Napi::Value
struct NapiConverter {
    static Napi::Value Convert(Napi::Env env, int value) {
        return Napi::Number::New(env, value);
    }
    
    static Napi::Value Convert(Napi::Env env, double value) {
        return Napi::Number::New(env, value);
    }
    
    static Napi::Value Convert(Napi::Env env, float value) {
        return Napi::Number::New(env, static_cast<double>(value));
    }
    
    static Napi::Value Convert(Napi::Env env, bool value) {
        return Napi::Boolean::New(env, value);
    }
    
    static Napi::Value Convert(Napi::Env env, const std::string& value) {
        return Napi::String::New(env, value);
    }
    
    static Napi::Value Convert(Napi::Env env, const char* value) {
        return Napi::String::New(env, value);
    }
    
    // For any other arithmetic type
    template<typename T>
    static typename std::enable_if<std::is_arithmetic<T>::value && 
                                   !std::is_same<T, int>::value && 
                                   !std::is_same<T, double>::value && 
                                   !std::is_same<T, float>::value && 
                                   !std::is_same<T, bool>::value, Napi::Value>::type
    Convert(Napi::Env env, T value) {
        return Napi::Number::New(env, static_cast<double>(value));
    }
    
    // For any other type, try to convert to string
    template<typename T>
    static typename std::enable_if<!std::is_arithmetic<T>::value && 
                                   !std::is_same<T, std::string>::value && 
                                   !std::is_same<T, const char*>::value, Napi::Value>::type
    Convert(Napi::Env env, const T& value) {
        return Napi::String::New(env, std::to_string(value));
    }
};

// Helper to convert tuple elements to vector recursively
template<int Index, typename... Args>
struct TupleConverter {
    static void Convert(std::vector<napi_value>& result, Napi::Env env, const std::tuple<Args...>& tuple) {
        TupleConverter<Index - 1, Args...>::Convert(result, env, tuple);
        result.push_back(NapiConverter::Convert(env, std::get<Index>(tuple)));
    }
};

// Base case for recursion
template<typename... Args>
struct TupleConverter<-1, Args...> {
    static void Convert(std::vector<napi_value>& result, Napi::Env env, const std::tuple<Args...>& tuple) {
        // Do nothing - base case
    }
};

// Main AsyncFn class template
template<typename... Args>
class AsyncFn : public Napi::AsyncWorker
{
    std::function<std::tuple<Args...>()> fn;
    std::tuple<Args...> ret;

public:
    AsyncFn(Napi::Function& callback, std::function<std::tuple<Args...>()> fn)
        : Napi::AsyncWorker(callback), fn(fn) {}

    void Execute() override
    {
        ret = fn();
    }

    void OnOK() override
    {
        std::vector<napi_value> args;
        args.reserve(sizeof...(Args));
        TupleConverter<sizeof...(Args) - 1, Args...>::Convert(args, Env(), ret);
        Callback().Call(args);
    }
};

// Convenience function for creating AsyncFn instances
template<typename... Args>
AsyncFn<Args...>* createAsyncFn(Napi::Function& callback, std::function<std::tuple<Args...>()> fn) {
    return new AsyncFn<Args...>(callback, fn);
}