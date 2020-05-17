#include "js.h"
#include <mutex>
#include <v8.h>
#include <libplatform/libplatform.h>

thread_local v8::Isolate *isolate = nullptr;

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

void javascript_thread_initialize()
{
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate = v8::Isolate::New(create_params);
}

std::string javascript_run(const char *rgch, size_t cch)
{
    if (isolate == nullptr)
        javascript_thread_initialize();

    v8::TryCatch trycatch(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    // Create a stack-allocated handle scope.
    v8::HandleScope handle_scope(isolate);
    // Create a new context.
    v8::Local<v8::Context> context = v8::Context::New(isolate);
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
            v8::String::Utf8Value estr(isolate, trycatch.Exception());
            throw std::string(*estr);
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
            v8::String::Utf8Value estr(isolate, trycatch.Exception());
            throw std::string(*estr);
        }
        throw std::nullptr_t();
    }
    
    // Convert the result to an UTF8 string and print it.
    v8::String::Utf8Value utf8(isolate, result);
    return std::string(*utf8);
}


void javascript_thread_shutdown()
{
    isolate->Dispose();
}
