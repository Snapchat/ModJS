#include "js.h"
#include "redismodule.h"
#include <limits.h>
#include <vector>
#include <v8.h>
#include <math.h>
#include <fstream>
#include <streambuf>
#include <dlfcn.h>
#include <experimental/filesystem>

RedisModuleCtx *g_ctx = nullptr;
JSContext *g_jscontext = nullptr;
bool g_fInStartup = true;

class KeyDBContext
{
    RedisModuleCtx *m_ctxSave;
public:
    KeyDBContext(RedisModuleCtx *ctxSet)
    {
        m_ctxSave = g_ctx;
        g_ctx = ctxSet;
    }

    ~KeyDBContext()
    {
        g_ctx = m_ctxSave;
    }
};

static void processResult(RedisModuleCtx *ctx, v8::Isolate *isolate, v8::Local<v8::Context> &v8ctx, v8::Local<v8::Value> &result)
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
                processResult(ctx, isolate, v8ctx, val);
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
    else if (result->IsString())
    {
        v8::String::Utf8Value utf8(isolate, result);
        RedisModule_ReplyWithCString(ctx, *utf8);
    }
    else
    {
        RedisModule_ReplyWithNull(ctx);
    }
}

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

void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) 
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    if (args.Length() != 2)
    {
        isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Log expects two parameters").ToLocalChecked());
        return;
    }
    
    
    v8::String::Utf8Value level(isolate, args[0]);
    v8::String::Utf8Value message(isolate, args[1]);
    RedisModule_Log(g_ctx, *level, "%s", *message);
}

int js_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    KeyDBContext ctxsav(ctx);

    v8::Locker locker(g_jscontext->getIsolate());
    v8::HandleScope scope(g_jscontext->getIsolate());
    if (argc < 1)
        return REDISMODULE_ERR;

    v8::Local<v8::Context> context = g_jscontext->getCurrentContext();
    v8::Context::Scope context_scope(context);
    
    try
    {
        size_t cchName;
        const char *rgchName = RedisModule_StringPtrLen(argv[0], &cchName);

        v8::Handle<v8::Object> global = context->Global();
        v8::Isolate *isolate = g_jscontext->getIsolate();
        auto strFn = v8::String::NewFromUtf8(isolate, rgchName, v8::NewStringType::kNormal, cchName).ToLocalChecked();
        auto mvfnCall = global->Get(context, v8::Local<v8::Value>::Cast(strFn));
        v8::Handle<v8::Value> vfnCall;
        if (!mvfnCall.ToLocal(&vfnCall))
            return REDISMODULE_ERR;
        
        if (!vfnCall->IsFunction())
            return REDISMODULE_ERR;
        
        v8::Handle<v8::Function> fnCall = v8::Handle<v8::Function>::Cast(vfnCall);
        std::vector<v8::Local<v8::Value>> vecargs;
        vecargs.reserve(argc);
        
        for (int iarg = 1; iarg < argc; ++iarg)
        {
            size_t cch;
            const char *rgch = RedisModule_StringPtrLen(argv[iarg], &cch);
            auto str = v8::String::NewFromUtf8(isolate, rgch, v8::NewStringType::kNormal, cch).ToLocalChecked();

            vecargs.push_back(str);
        }

        auto maybeResult = fnCall->Call(context, global, (int)vecargs.size(), vecargs.data());

        v8::Local<v8::Value> result;
        if (!maybeResult.ToLocal(&result))
        {
            RedisModule_ReplyWithNull(ctx);
            return REDISMODULE_OK;
        }

        processResult(ctx, g_jscontext->getIsolate(), context, result);
    }
    catch (std::string strerr)
    {
        RedisModule_ReplyWithError(ctx, strerr.c_str());
        return REDISMODULE_ERR;
    }
    catch (std::nullptr_t)
    {
        RedisModule_ReplyWithError(ctx, "Unknown Error");
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

void RegisterCommandCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1) return;
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    if (!g_fInStartup)
    {
        isolate->ThrowException(v8::String::NewFromUtf8(isolate, "New commands may only be registered during startup").ToLocalChecked());
        return;
    }

    v8::Local<v8::Value> vfn = args[0];
    if (!vfn->IsFunction())
        return;

    v8::Local<v8::Function> fn = v8::Local<v8::Function>::Cast(vfn);
    v8::Local<v8::Value> vfnName = fn->GetName();
    v8::String::Utf8Value fnName(isolate, vfnName);

    std::string flags = "write deny-oom random";
    if (args.Length() >= 2)
    {
        // They passed in their own flags
        v8::Local<v8::Value> vFlags = args[1];
        if (vFlags->IsString())
        {
            auto utfFlags = v8::String::Utf8Value(isolate, vFlags);
            flags = *utfFlags;
        }
    }

    int keyFirst = 0;
    int keyLast = 0;
    int keyStep = 0;
    if (args.Length() > 2)
    {
        if (args.Length() != 5)
        {
            isolate->ThrowException(v8::String::NewFromUtf8(isolate, "incorrect number of arguments to register()").ToLocalChecked());
            return;
        }

        if (!args[2]->IsInt32() || !args[3]->IsInt32() || !args[4]->IsInt32())
        {
            isolate->ThrowException(v8::String::NewFromUtf8(isolate, "expected integer argument").ToLocalChecked());
            return;
        }

        keyFirst = v8::Local<v8::Int32>::Cast(args[2])->Value();
        keyLast = v8::Local<v8::Int32>::Cast(args[3])->Value();
        keyStep = v8::Local<v8::Int32>::Cast(args[4])->Value();
    }

    if (RedisModule_CreateCommand(g_ctx, *fnName, js_command, flags.c_str(), keyFirst, keyLast, keyStep) == REDISMODULE_ERR) {
        isolate->ThrowException(v8::String::NewFromUtf8(isolate, "failed to register command").ToLocalChecked());
    }

    RedisModule_Log(g_ctx, "verbose", "Function %s registered", *fnName);
}

int evaljs_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    KeyDBContext ctxsav(ctx);

    if (argc != 2)
    {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    if (g_jscontext == nullptr)
    {
        g_jscontext = new JSContext();
        g_jscontext->initialize();
    }

    v8::Locker locker(g_jscontext->getIsolate());

    size_t cch = 0;
    const char *rgch = RedisModule_StringPtrLen(argv[1], &cch);
    try
    {
        v8::HandleScope scope(g_jscontext->getIsolate());
        v8::Local<v8::Value> result = g_jscontext->run(rgch, cch);
        auto context = g_jscontext->getCurrentContext();
        processResult(ctx, g_jscontext->getIsolate(), context, result);
    }
    catch (std::string strerr)
    {
        RedisModule_ReplyWithError(ctx, strerr.c_str());
        return REDISMODULE_ERR;
    }
    catch (std::nullptr_t)
    {
        RedisModule_ReplyWithError(ctx, "Unknown Error");
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

int run_startup_script(RedisModuleCtx *ctx, const char *rgchPath, size_t cchPath)
{
    KeyDBContext ctxsav(ctx);

    std::ifstream file(rgchPath, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    if (size == -1)
    {
        RedisModule_Log(ctx, "warning", "startup script does not exist");
        return REDISMODULE_ERR; // Failed to read file
    }

    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size))
    {
        RedisModule_Log(ctx, "warning", "failed to read startup script");
        return REDISMODULE_ERR; // Failed to read file
    }

    v8::HandleScope scope(g_jscontext->getIsolate());
    try
    {
        g_jscontext->run(buffer.data(), buffer.size(), true /* don't cache */);
    }
    catch (std::string str)
    {
        RedisModule_Log(ctx, "warning", "%s", str.c_str());
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

extern "C" int __attribute__((visibility("default"))) RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    printf("argc: %d\n", argc);
    if (RedisModule_Init(ctx,"modjs",1,REDISMODULE_APIVER_1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx,"evaljs", evaljs_command,"write deny-oom random",0,0,0) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    javascript_initialize();

    g_jscontext = new JSContext();
    g_jscontext->initialize();

    // Run our bootstrap.js code
    {
        Dl_info dlInfo;
        dladdr((const void*)RedisModule_OnLoad, &dlInfo);
        if (dlInfo.dli_sname != NULL && dlInfo.dli_saddr != NULL)
        {
            std::experimental::filesystem::path path(dlInfo.dli_fname);
            path.remove_filename();
            path /= "bootstrap.js";
            std::string strPath = path.string();
            if (run_startup_script(ctx, strPath.data(), strPath.size()) == REDISMODULE_ERR)
            {
                RedisModule_Log(ctx, "warning", "failed to run bootstrap.js, ensure this is located in the same location as the .so");
                return REDISMODULE_ERR;
            }
        }
        else
        {
            RedisModule_Log(ctx, "warning", "failed to locate bootstrap script");
            return REDISMODULE_ERR;
        }
    }

    for (int iarg = 0; iarg < argc; ++iarg)
    {
        // Process the startup script
        size_t cchPath;
        const char *rgchPath = RedisModule_StringPtrLen(argv[iarg], &cchPath);

        if (run_startup_script(ctx, rgchPath, cchPath) == REDISMODULE_ERR)
            return REDISMODULE_ERR;
    }

    g_fInStartup = false;
    return REDISMODULE_OK;
}
