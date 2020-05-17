#pragma once

#include <string>

void javascript_initialize();
void javascript_thread_initialize();
void javascript_shutdown();
void javascript_thread_shutdown();

std::string javascript_run(const char *rgch, size_t cch);