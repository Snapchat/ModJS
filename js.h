#pragma once

#include <string>
#include <v8.h>

void javascript_initialize();
void javascript_thread_initialize();
void javascript_shutdown();
void javascript_thread_shutdown();

v8::Local<v8::Value> javascript_run(v8::Local<v8::Context> &context, const char *rgch, size_t cch);