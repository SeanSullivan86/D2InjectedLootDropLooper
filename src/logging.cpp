#include "logging.h"
#include <windows.h>
#include <stdio.h>

#pragma warning(disable : 4996)

HANDLE logMutex;
FILE* logFile;

void Logging_init(const char* logFilePath) {
    logFile = fopen(logFilePath, "ab");
    logMutex = CreateSemaphore(NULL, 1, 1, 0);
}

void logPrintf(_Printf_format_string_ char const* const format, ...) {
    va_list args;
    va_start(args, format);

    WaitForSingleObject(logMutex, UINT_MAX /* timeout */);

    vfprintf(logFile, format, args);
    fflush(logFile);

    ReleaseSemaphore(logMutex, 1, NULL);

    va_end(args);
}

void Logging_close() {
    fflush(logFile);
    fclose(logFile);
}