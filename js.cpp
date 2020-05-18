#include "js.h"
#include <mutex>
#include <libplatform/libplatform.h>
#include <fstream>
#include <streambuf>

void KeyDBExecuteCallback(const v8::FunctionCallbackInfo<v8::Value>& args);

thread_local v8::Isolate *isolate = nullptr;
thread_local v8::Persistent<v8::ObjectTemplate, v8::CopyablePersistentTraits<v8::ObjectTemplate>> tls_global;


void javascript_initialize()
{
    v8::V8::InitializeICUDefaultLocation("keydb-server");
    v8::V8::InitializeExternalStartupData("keydb-server");
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    platform.release();
}

void javascript_shutdown()
{
    v8::V8::Dispose();
}

class Script
{
    v8::Local<v8::Script> script;
    v8::Local<v8::Context> context;
};

static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) 
{
    if (args.Length() < 1) return;
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> arg = args[0];
    v8::String::Utf8Value value(isolate, arg);
    printf("%s\n", *value);
}


void javascript_hooks_initialize(v8::Local<v8::ObjectTemplate> &keydb_obj)
{
    keydb_obj->Set(v8::String::NewFromUtf8(isolate, "log", v8::NewStringType::kNormal)
        .ToLocalChecked(),
        v8::FunctionTemplate::New(isolate, LogCallback));

    keydb_obj->Set(v8::String::NewFromUtf8(isolate, "call", v8::NewStringType::kNormal)
        .ToLocalChecked(),
        v8::FunctionTemplate::New(isolate, KeyDBExecuteCallback));
}

void javascript_thread_initialize()
{
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate = v8::Isolate::New(create_params);

    v8::HandleScope handle_scope(isolate);

    // Create a template for the global object where we set the
    // built-in global functions.
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    v8::Local<v8::ObjectTemplate> keydb_obj = v8::ObjectTemplate::New(isolate);

    javascript_hooks_initialize(keydb_obj);

    global->Set(v8::String::NewFromUtf8(isolate, "keydb", v8::NewStringType::kNormal)
                    .ToLocalChecked(),
                keydb_obj);
    global->Set(v8::String::NewFromUtf8(isolate, "redis", v8::NewStringType::kNormal)
                    .ToLocalChecked(),
                keydb_obj);

    tls_global = v8::Persistent<v8::ObjectTemplate, v8::CopyablePersistentTraits<v8::ObjectTemplate>>(isolate, global);
}

std::string prettyPrintException(v8::TryCatch &trycatch)
{
    auto e = trycatch.Exception();
    v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, isolate->GetCurrentContext());
    v8::String::Utf8Value estr(isolate, e);
    std::string str(*estr);

    auto maybeTrace = trycatch.StackTrace(context);
    v8::Local<v8::Value> traceV;

    if (maybeTrace.ToLocal(&traceV))
    {
        str += "\n";
        str += *v8::String::Utf8Value(isolate, traceV);
    }
    return str;
}

v8::Local<v8::Value> javascript_run(v8::Local<v8::Context> &context, const char *rgch, size_t cch)
{
    v8::TryCatch trycatch(isolate);
    
    // Enter the context for compiling and running the hello world script.
    v8::Context::Scope context_scope(context);
    // Create a string containing the JavaScript source code.
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(isolate, rgch,
                                v8::NewStringType::kNormal, cch)
            .ToLocalChecked();
    // Compile the source code.
    v8::Local<v8::Script> script;
    auto scriptMaybe =
        v8::Script::Compile(context, source);

    if (!scriptMaybe.ToLocal(&script))
    {
        if (trycatch.HasCaught())
        {
            throw prettyPrintException(trycatch);
        }
        throw std::nullptr_t();
    }
    
    // Run the script to get the result.
    v8::Local<v8::Value> result;
    auto resultMaybe = script->Run(context);

    if (!resultMaybe.ToLocal(&result))
    {
        if (trycatch.HasCaught())
        {
            throw prettyPrintException(trycatch);
        }
        throw std::nullptr_t();
    }
    
    // Convert the result to a KeyDB type and return it
    return result;
}


void javascript_thread_shutdown()
{
    isolate->Dispose();
}
