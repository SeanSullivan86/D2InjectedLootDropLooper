#pragma once
#include <cstdint>

typedef unsigned char byte;


byte* __fastcall ShortTermMemoryAllocator_Malloc(void* pMemoryPool, uint32_t requestedSize, char* callingFilename, uint32_t lineNumber);

void __fastcall ShortTermMemoryAllocator_Free(void* pMemoryPool, byte** memoryLocationToFree, char* callingFilename, uint32_t lineNumber);

byte* __fastcall ShortTermMemoryAllocator_Realloc(void* pMemoryPool, byte* previouslyAllocated, uint32_t newSize, char* callingFilename, uint32_t lineNumber);

void ShortTermMemoryAllocator_validateAllMemoryFreed();

void ShortTermMemoryAllocator_resetSimpleMemoryAllocator();

extern unsigned long ShortTermMemoryAllocator_SleepAllThreadsExceptThisOne;