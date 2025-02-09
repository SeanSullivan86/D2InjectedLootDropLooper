#pragma once
typedef unsigned char BYTE;



struct BoundedBuffer;

struct BoundedBuffer* BoundedBuffer_create(int bufferSize, int sizeOfEachBufferSlot);

BYTE* BoundedBuffer_AcquireProducerSlot(struct BoundedBuffer* buffer);

BYTE* BoundedBuffer_AcquireConsumerSlot(struct BoundedBuffer* buffer, int* bytesToConsume);

void BoundedBuffer_ReleaseProducerSlot(struct BoundedBuffer* buffer, int bytesWritten);

void BoundedBuffer_ReleaseConsumerSlot(struct BoundedBuffer* buffer);