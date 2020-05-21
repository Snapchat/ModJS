#include <cstddef>  // std::size_t
#include <new>
#include <stdio.h>

extern void *(*RedisModule_Alloc)(size_t bytes);
extern void (*RedisModule_Free)(void *ptr);
extern void *(*RedisModule_Calloc)(size_t nmemb, size_t size);

void *operator new(size_t size)
{
    return RedisModule_Alloc(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    return RedisModule_Alloc(size);
}

void operator delete(void * p) noexcept
{
    RedisModule_Free(p);
}

void operator delete(void *p, std::size_t) noexcept
{
    RedisModule_Free(p);
}

void operator delete  (void* p, const std::nothrow_t& ) noexcept
{
    RedisModule_Free(p);
}