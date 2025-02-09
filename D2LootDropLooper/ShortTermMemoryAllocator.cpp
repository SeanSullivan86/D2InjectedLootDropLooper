#include <windows.h>
#include <cstdint>
#include "logging.h"

#pragma warning(disable : 4996)

byte simpleMemoryRegion[1000000];
byte* pointers[10000];
byte* freedPointers[10000];
uint32_t nextMemoryOffsetToAllocate = 0;
uint32_t pointerCount = 0;
uint32_t freedPointerCount = 0;

char callingFilenames[10000][100];
uint32_t callingLineNumbers[10000];
uint32_t pointerSizes[10000];

unsigned long ShortTermMemoryAllocator_SleepAllThreadsExceptThisOne;

byte* __fastcall ShortTermMemoryAllocator_Malloc(void* pMemoryPool, uint32_t requestedSize, char* callingFilename, uint32_t lineNumber) {

    //fprintf(logFile, "simpleMalloc %p %d %s %d \n", pMemoryPool, requestedSize, callingFilename, lineNumber);
    //fflush(logFile);

    DWORD threadId = GetCurrentThreadId();
    if (threadId != ShortTermMemoryAllocator_SleepAllThreadsExceptThisOne) {
        logPrintf("simpleMalloc: Putting thread %d to sleep \n", threadId);
        while (1) { Sleep(4000000000); }
    }

    pointers[pointerCount] = &simpleMemoryRegion[nextMemoryOffsetToAllocate];
    //fprintf(logFile, "Created pointer %d %p at offset %d \n", pointerCount, pointers[pointerCount], nextMemoryOffsetToAllocate);
    //fflush(logFile);

    nextMemoryOffsetToAllocate += requestedSize;

    strcpy(callingFilenames[pointerCount], callingFilename);
    callingLineNumbers[pointerCount] = lineNumber;
    pointerSizes[pointerCount] = requestedSize;

    pointerCount++;

    return pointers[pointerCount - 1];
}



void __fastcall ShortTermMemoryAllocator_Free(void* pMemoryPool, byte** memoryLocationToFree, char* callingFilename, uint32_t lineNumber) {

    //fprintf(logFile, "simpleFree %p %p %s %d \n", pMemoryPool, *memoryLocationToFree, callingFilename, lineNumber);
    //fflush(logFile);

    DWORD threadId = GetCurrentThreadId();
    if (threadId != ShortTermMemoryAllocator_SleepAllThreadsExceptThisOne) {
        logPrintf("simpleFree: Putting thread %d to sleep \n", threadId);
        while (1) { Sleep(2000000000); }
    }

    freedPointers[freedPointerCount] = *memoryLocationToFree;
    freedPointerCount++;
}

byte* __fastcall ShortTermMemoryAllocator_Realloc(void* pMemoryPool, byte* previouslyAllocated, uint32_t newSize, char* callingFilename, uint32_t lineNumber) {
    //fprintf(logFile, "simpleRealloc %p %p %d %s %d \n", pMemoryPool, previouslyAllocated, newSize, callingFilename, lineNumber);
    //fflush(logFile);

    if (previouslyAllocated == NULL) {
        //fprintf(logFile, "simpleRealloc with previouslyAllocated=null, delegating to malloc \n");
        //fflush(logFile);
        return ShortTermMemoryAllocator_Malloc(pMemoryPool, newSize, callingFilename, lineNumber);
    }

    byte* newAllocation = ShortTermMemoryAllocator_Malloc(pMemoryPool, newSize, callingFilename, lineNumber);

    // warning: we're copying all newSize bytes from the previously location despite not knowing the previous size. it can crash.
    if (previouslyAllocated < newAllocation && (previouslyAllocated + newSize > newAllocation)) {
        /* fprintf(logFile, "previouslyAllocated overlaps with newAllocation. oldLocation:%p newLocation:%p newSize:%d , modifiedBytesToCopy:%d \n",
            previouslyAllocated, newAllocation, newSize, newAllocation - previouslyAllocated);
        fflush(logFile); */
        memcpy(newAllocation, previouslyAllocated, newAllocation - previouslyAllocated);
    }
    else {
        memcpy(newAllocation, previouslyAllocated, newSize);
    }


    ShortTermMemoryAllocator_Free(pMemoryPool, &previouslyAllocated, callingFilename, lineNumber);

    return newAllocation;
}

void ShortTermMemoryAllocator_validateAllMemoryFreed() {
    logPrintf("validateAllMemoryFreed: Pointer Count %d , Freed Pointer Count %d \n", pointerCount, freedPointerCount);

    if (pointerCount != freedPointerCount) {
        logPrintf("Pointer Count %d , Freed Pointer Count %d \n", pointerCount, freedPointerCount);
    }

    int wasFreed = 0;
    for (int i = 0; i < pointerCount; i++) {
        wasFreed = 0;
        for (int j = 0; j < freedPointerCount; j++) {
            if (pointers[i] == freedPointers[j]) {
                wasFreed = 1;
                break;
            }
        }
        if (!wasFreed) {
            logPrintf("Allocated but not freed : %p size=%d filename=%s lineNumber=%d \n", ((void*)(pointers[i])), pointerSizes[i], callingFilenames[i], callingLineNumbers[i]);
        }
    }

    logPrintf("exiting validateAllMemoryFreed \n");
}

void ShortTermMemoryAllocator_resetSimpleMemoryAllocator() {
    nextMemoryOffsetToAllocate = 0;
    pointerCount = 0;
    freedPointerCount = 0;
}