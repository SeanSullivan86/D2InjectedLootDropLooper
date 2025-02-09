#include "boundedBuffer.h"
#include <windows.h>

struct BoundedBuffer {
    HANDLE freeSlotSemaphore;
    HANDLE fullSlotSemaphore;
    int bufferSize;
    int sizeOfEachBufferSlot;
    BYTE* bufferData;
    int* queueDataSizes;
    int producerSlot;
    int consumerSlot;
};

struct BoundedBuffer* BoundedBuffer_create(int bufferSize, int sizeOfEachBufferSlot) {
    struct BoundedBuffer* buffer = (struct BoundedBuffer*) malloc(sizeof(struct BoundedBuffer));

    buffer->freeSlotSemaphore = CreateSemaphore(NULL, bufferSize, LONG_MAX, NULL);
    buffer->fullSlotSemaphore = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
    buffer->bufferSize = bufferSize;
    buffer->sizeOfEachBufferSlot = sizeOfEachBufferSlot;
    buffer->producerSlot = 0;
    buffer->consumerSlot = 0;
    buffer->bufferData = (BYTE*)malloc(bufferSize * sizeOfEachBufferSlot);
    buffer->queueDataSizes = (int*)malloc(bufferSize * sizeof(int));

    return buffer;
}

BYTE* BoundedBuffer_AcquireProducerSlot(struct BoundedBuffer* buffer) {
    WaitForSingleObject(buffer->freeSlotSemaphore, UINT_MAX);

    return buffer->bufferData + (buffer->sizeOfEachBufferSlot * buffer->producerSlot);
}


BYTE* BoundedBuffer_AcquireConsumerSlot(struct BoundedBuffer* buffer, _Out_ int* bytesToConsume) {
    WaitForSingleObject(buffer->fullSlotSemaphore, UINT_MAX);

    *bytesToConsume = buffer->queueDataSizes[buffer->consumerSlot];
    return buffer->bufferData + (buffer->sizeOfEachBufferSlot * buffer->consumerSlot);
   
}

void BoundedBuffer_ReleaseProducerSlot(struct BoundedBuffer* buffer, int bytesWritten) {

    ReleaseSemaphore(buffer->fullSlotSemaphore, 1, NULL);
    
    buffer->queueDataSizes[buffer->producerSlot] = bytesWritten;

    buffer->producerSlot++;
    if (buffer->producerSlot == buffer->bufferSize) buffer->producerSlot = 0;
}

void BoundedBuffer_ReleaseConsumerSlot(struct BoundedBuffer* buffer) {
    ReleaseSemaphore(buffer->freeSlotSemaphore, 1, NULL);

    buffer->consumerSlot++;
    if (buffer->consumerSlot == buffer->bufferSize) buffer->consumerSlot = 0;
}