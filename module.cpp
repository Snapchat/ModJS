#include "js.h"
#include "redismodule.h"
#include <limits.h>
#include <vector>
#include <v8.h>
#include <math.h>

thread_local RedisModuleCtx *g_ctx = nullptr;
extern thread_local v8::Isolate *isolate;
extern thread_local v8::Persistent<v8::ObjectTemplate, v8::CopyablePersistentTraits<v8::ObjectTemplate>> tls_global;

static void ProcessCallReply(v8::Local<v8::Value> &dst, v8::Isolate* isolate, RedisModuleCallReply *reply)
{
    const char *rgchReply;
    size_t cchReply;

    switch (RedisModule_CallReplyType(reply))
    {
        case REDISMODULE_REPLY_STRING:
            rgchReply = RedisModule_CallReplyStringPtr(reply, &cchReply);
            dst = v8::String::NewFromUtf8(isolate, rgchReply, v8::NewStringType::kNormal, cchReply).ToLocalChecked();
            break;
        
        case REDISMODULE_REPLY_INTEGER:
            {
            long long val =  RedisModule_CallReplyInteger(reply);
            dst = v8::Number::New(isolate, (double)val);
            break;
            }

        case REDISMODULE_REPLY_ARRAY:
            {
            size_t celem = RedisModule_CallReplyLength(reply);

            auto array = v8::Array::New(isolate, celem);
            for (size_t ielem  = 0; ielem < celem; ++ielem)
            {
                RedisModuleCallReply *replyArray = RedisModule_CallReplyArrayElement(reply, ielem);
                v8::Local<v8::Value> val;
                ProcessCallReply(val, isolate, replyArray);
                v8::Maybe<bool> result = array->Set(isolate->GetCurrentContext(), ielem, val);
                bool fResult;
                if (!result.To(&fResult) || !fResult)
                {
                    RedisModule_Log(g_ctx, "warning", "Failed to process array result");
                }
            }
            dst = array;
            break;
            }

        default:
            rgchReply = RedisModule_CallReplyProto(reply, &cchReply);
            dst = v8::String::NewFromUtf8(isolate, rgchReply, v8::NewStringType::kNormal, cchReply).ToLocalChecked();
    }
}

void KeyDBExecuteCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1) return;
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> vfnName = args[0];
    v8::String::Utf8Value fnName(isolate, vfnName);

    std::vector<RedisModuleString*> vecstrs;
    for (size_t iarg = 1; iarg < args.Length(); ++iarg)
    {
        v8::String::Utf8Value argument(isolate, args[iarg]);
        vecstrs.push_back(RedisModule_CreateString(g_ctx, *argument, argument.length()));
    }

    RedisModuleCallReply *reply = RedisModule_Call(g_ctx, *fnName, "v", vecstrs.data(), vecstrs.size());

    if (reply != nullptr)
    {
        v8::Local<v8::Value> result;
        ProcessCallReply(result, isolate, reply);
        args.GetReturnValue().Set(result);

        RedisModule_FreeCallReply(reply);
    }
    else
    {
        isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Invalid Command").ToLocalChecked());
    }

    for (auto str : vecstrs)
        RedisModule_FreeString(g_ctx, str);
}


static void processResult(RedisModuleCtx *ctx, v8::Local<v8::Context> &v8ctx, v8::Local<v8::Value> &result)
{
    if (result->IsArray())
    {
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(result);
        RedisModule_ReplyWithArray(g_ctx, array->Length());
        for (size_t ielem = 0; ielem < array->Length(); ++ielem)
        {
            auto maybe = array->Get(v8ctx, ielem);
            v8::Local<v8::Value> val;
            if (maybe.ToLocal(&val))
                processResult(ctx, v8ctx, val);
            else
                RedisModule_ReplyWithNull(ctx);
        }
    }
    else if (result->IsInt32())
    {
        v8::Local<v8::Int32> num = v8::Local<v8::Int32>::Cast(result);
        int32_t val = num->Value();
        RedisModule_ReplyWithLongLong(ctx, val);
    }
    else if (result->IsNumber())
    {
        v8::Local<v8::Number> num = v8::Local<v8::Number>::Cast(result);
        double dv = num->Value();
        RedisModule_ReplyWithDouble(ctx, dv);
    }
    else
    {
        v8::String::Utf8Value utf8(isolate, result);
        RedisModule_ReplyWithCString(ctx, *utf8);
    }
}

int evaljs_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2)
    {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    if (isolate == nullptr)
        javascript_thread_initialize();

    size_t cch = 0;
    const char *rgch = RedisModule_StringPtrLen(argv[1], &cch);
    try
    {
        g_ctx = ctx;
        v8::Isolate::Scope isolate_scope(isolate);
        // Create a stack-allocated handle scope.
        v8::HandleScope handle_scope(isolate);
        // Create a new context.
        v8::Local<v8::ObjectTemplate> global = v8::Local<v8::ObjectTemplate>::New(isolate, tls_global);
        v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
        v8::Local<v8::Value> result = javascript_run(context, rgch, cch);
        processResult(ctx, context, result);
    }
    catch (std::string strerr)
    {
        RedisModule_ReplyWithError(ctx, strerr.c_str());
        g_ctx = nullptr;
        return REDISMODULE_ERR;
    }
    catch (std::nullptr_t)
    {
        RedisModule_ReplyWithError(ctx, "Unknown Error");
        g_ctx = nullptr;
        return REDISMODULE_ERR;
    }
    g_ctx = nullptr;
    return REDISMODULE_OK;
}

extern "C" int __attribute__((visibility("default"))) RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (RedisModule_Init(ctx,"modjs",1,REDISMODULE_APIVER_1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx,"evaljs", evaljs_command,"",0,0,0) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    javascript_initialize();
    return REDISMODULE_OK;
}