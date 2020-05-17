#include "js.h"
#include "redismodule.h"

int evaljs_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (argc != 2)
    {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    size_t cch = 0;
    const char *rgch = RedisModule_StringPtrLen(argv[1], &cch);
    try
    {
        std::string strRes = javascript_run(rgch, cch);
        RedisModule_ReplyWithCString(ctx, strRes.c_str());
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

extern "C" int __attribute__((visibility("default"))) RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc)
{
    if (RedisModule_Init(ctx,"modjs",1,REDISMODULE_APIVER_1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx,"evaljs", evaljs_command,"",0,0,0) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    javascript_initialize();
    return REDISMODULE_OK;
}