#pragma once

#include <string>
#include <v8.h>

class HotScript;
class JSContext
{
    v8::Isolate *isolate = nullptr;
    v8::Persistent<v8::ObjectTemplate, v8::CopyablePersistentTraits<v8::ObjectTemplate>> m_global;
    v8::Persistent<v8::Context, v8::CopyablePersistentTraits<v8::Context>> m_context;

public:
    JSContext();
    ~JSContext();

    void initialize();
    v8::Local<v8::Value> run(const char *rgch, size_t cch, bool fNoCache = false);
    v8::Local<v8::Context> getCurrentContext() { return v8::Local<v8::Context>::New(isolate, m_context); }
    v8::Isolate *getIsolate() { return isolate; }

protected:
    v8::Local<v8::Value> run(v8::Local<v8::Context> &context, v8::Local<v8::Script> &script);
    std::string prettyPrintException(v8::TryCatch &trycatch);
    void javascript_hooks_initialize(v8::Local<v8::ObjectTemplate> &keydb_obj);
    
    static void RequireCallback(const v8::FunctionCallbackInfo<v8::Value>& args);

    std::unique_ptr<HotScript> m_sphotscript;
};

void javascript_initialize();
void javascript_shutdown();