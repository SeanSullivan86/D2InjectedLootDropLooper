#pragma once

#include <sal.h>

void Logging_init(const char* logFilePath);

void logPrintf(_Printf_format_string_ char const* const format, ...);

void Logging_close();