#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "logging.h"
#include "boundedBuffer.h"
#include "socketio.h"
#include "D2GameFunctions.h"
#include "ShortTermMemoryAllocator.h"

#pragma warning(disable : 4996) // strcpy unsafe warning


HANDLE socketWriterThread;

#define OUTPUT_PORT "5492"


#ifndef ArraySize
#define ArraySize(x) (sizeof((x)) / sizeof((x)[0]))
#endif


BoundedBuffer* bufferToHandoffDataToSocketWriterThread;

uint32_t* newItems;
int newItemCount;


typedef struct PatchHook_t
{
    DWORD originalAddress;
    DWORD addressOfNewCodeToRun;
    DWORD lengthOfInstructionsToOverwrite;
    //BYTE *bOldCode;
} PatchHook;

void utf16toString(char from[], char to[]) {
    int i = 0;
    for (i = 0; from[2 * i] != 0; i++) {
        to[i] = from[2 * i];
    }
}

void utf16toString_newlines_as_commas(char from[], char to[]) {
    int i = 0;
    for (i = 0; from[2 * i] != 0; i++) {
        if (from[2 * i] == '\n') {
            to[i] = ',';
        }
        else {
            to[i] = from[2 * i];
        }
    }
}

static const unsigned char base64_table[65] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
void base64_encode(BYTE* src, size_t len, BYTE* out, size_t* out_len)
{
    BYTE* pos;
    BYTE* end, * in;

    end = src + len;
    in = src;
    pos = out;

    while (end - in >= 3) {
        *pos++ = base64_table[in[0] >> 2];
        *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64_table[in[2] & 0x3f];
        in += 3;
    }

    if (end - in) {
        *pos++ = base64_table[in[0] >> 2];
        if (end - in == 1) {
            *pos++ = base64_table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        }
        else {
            *pos++ = base64_table[((in[0] & 0x03) << 4) |
                (in[1] >> 4)];
            *pos++ = base64_table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    *pos = '\0';
    if (out_len)
        *out_len = pos - out;
}


char qualityStr[9][10] = { "", "Inferior", "Normal", "Superior", "Magic", "Set", "Rare", "Unique", "Crafted" };

char itemNameBuffer[200];
char asciiItemName[100];
char itemAttributesBuffer[2000];
char asciiItemAttributes[1000];

BYTE itemD2SBinary[500];
BYTE itemD2SBase64[800];



byte* sgptDataTable;
byte* pItemTypesTxtTable;
byte* itemTypesTxtTable;



DWORD WINAPI socketWriterThreadEntryPoint(LPVOID lpParam) {
    logPrintf("Start of socketWriterThreadFunc...");
    SocketIO_init(OUTPUT_PORT);
    logPrintf("Finished socket init...");
    
    int bytesToWrite = 0;
    BYTE* dataToWriteToSocket;
    while (true) {

        dataToWriteToSocket = BoundedBuffer_AcquireConsumerSlot(bufferToHandoffDataToSocketWriterThread, &bytesToWrite);

        SocketIO_write((char*) dataToWriteToSocket, bytesToWrite);

        BoundedBuffer_ReleaseConsumerSlot(bufferToHandoffDataToSocketWriterThread);

    }
    return 0;
}


uint64_t totalItemCount = 0;
LARGE_INTEGER qpcFrequency;
uint64_t previousNanos = 0;

void finishAndWriteItemEvent(void* pGame, uint64_t iteration) {
    byte* outputBuffer = BoundedBuffer_AcquireProducerSlot(bufferToHandoffDataToSocketWriterThread);

    int combinedSizeOfAllItems = 0;

    for (int i = 0; i < newItemCount; i++) {
        //fprintf(logFile, "Start iteration %llu item %d \n", iteration, i);
        //fflush(logFile);

        byte* pUnit = (byte*)(newItems[i]);

        uint32_t dwClassId = (*((uint32_t*)(pUnit + 0x04)));
        byte* pItemsTxt = D2Game_ItemTxt_GetById_006335f0(dwClassId);
        uint32_t itemTypeId = *((uint16_t*)(pItemsTxt + 286));
        byte* pItemType = itemTypesTxtTable + (228 * itemTypeId);
        byte* itemTypeCode = pItemType + 0;

        byte* itemData = (byte*)*(((uint32_t*)(pUnit + 0x14)));
        (*((uint32_t*)(itemData + 0x18))) |= 0x00000010; // set "identified" bit to true

        uint32_t itemCode = D2Game_getItemCode(pUnit);
        ((byte*)(&itemCode))[3] = 0;

        uint8_t isEthereal = (uint8_t)(((*((uint32_t*)(itemData + 0x18))) & 0x00400000) != 0);
        uint8_t sockets = (uint8_t)D2Game_getUnitStat(pUnit, 0x0C2, 0);
        uint8_t quality = (uint8_t) (*((uint32_t*)(itemData + 0x00)));

        memset(itemNameBuffer, 0, 200);
        memset(asciiItemName, 0, 100);
        memset(itemAttributesBuffer, 0, 2000);
        memset(asciiItemAttributes, 0, 1000);

        D2Game_getItemName(pUnit, (void*)(&itemNameBuffer), 0x100);
        utf16toString_newlines_as_commas(itemNameBuffer, asciiItemName);
        D2Game_getItemAttributes(pUnit, (void*)(&itemAttributesBuffer), 0x1000, 1, 0);
        utf16toString_newlines_as_commas(itemAttributesBuffer, asciiItemAttributes);

        //fprintf(logFile, "Item %p %s %s %s , sockets %d, quality %d\n", pUnit, asciiItemName, asciiItemAttributes, ((char*)(&itemCode)), sockets, quality);
        //fflush(logFile);


        byte *pStatListEx = D2Game_Unit_FindStatListByStateAndFlags(pUnit, 0, 0x40);
        byte* stats = nullptr;
        byte* statsArray = nullptr;

        uint16_t statCount = 0;
        if (pStatListEx != nullptr) {
            stats = pStatListEx + 0x24;
            statsArray = (byte*)*((uint32_t*)stats);
            statCount = *((uint16_t*)(stats + 0x04));
        }

        uint16_t d2sBinaryByteCount = D2Game_serializeItemToByteBuffer(pUnit, (void*)(&itemD2SBinary), 498, 1, 1, 0);
        // size_t d2sBase64ByteCount;
        // base64_encode(itemD2SBinary, d2sBinaryByteCount, itemD2SBase64, &d2sBase64ByteCount);

        uint32_t gold = D2Game_getUnitStat(pUnit, 14, 0);
        uint32_t defense = D2Game_getUnitStat(pUnit, 31, 0);

        // Data Format
        // ByteOffset(decimal) - Element Description
        // 00   Total Message Length (including these 2 bytes) (2 bytes)
        // 02   dropIteration number (8 bytes)
        // 10   itemCode (3 bytes)
        // 13   itemTypeCode (4 bytes)
        // 17   itemQuality (1 byte)
        // 18   isEthereal (1 byte)
        // 19   sockets (1 byte)
        // 20   gold (4 bytes)
        // 24   defense (4 bytes)
        // 
        // -- Defines the Sizes of the Variable Length Sections --
        // 28   itemNameByteLength (2 bytes)
        // 30   itemDescriptionByteLength (2 bytes)
        // 32   d2sByteLength (2 bytes)
        // 34   Count of Stats (2 bytes)
        // 
        // -- Variable Length Section --
        // 36   Stats (8 * statCount bytes)
        //      ItemName (not including \0 string terminator)
        //      ItemDescription (not including \0 string terminator)
        //      Item in D2S Binary Format

        // Total Length = 36 + 8*statCount + strlen(itemName) + strlen(itemDescription) + byteLength(d2s)

        uint16_t itemNameLength = strlen(asciiItemName);
        uint16_t itemDescriptionLength = strlen(asciiItemAttributes);
        uint16_t totalMessageLength = 36 + 8 * statCount + itemNameLength + itemDescriptionLength + d2sBinaryByteCount;

        //fprintf(logFile, "Start writing to internal  buffer, length %d\n", totalMessageLength);
       // fflush(logFile);

        memcpy(outputBuffer, &totalMessageLength, 2);
        memcpy(outputBuffer + 2, &iteration, 8);
        memcpy(outputBuffer + 10, ((byte*)(&itemCode)), 3);
        memcpy(outputBuffer + 13, itemTypeCode, 4);
        memcpy(outputBuffer + 17, &quality, 1);
        memcpy(outputBuffer + 18, &isEthereal, 1);
        memcpy(outputBuffer + 19, &sockets, 1);
        memcpy(outputBuffer + 20, &gold, 4);
        memcpy(outputBuffer + 24, &defense, 4);
        memcpy(outputBuffer + 28, &itemNameLength, 2);
        memcpy(outputBuffer + 30, &itemDescriptionLength, 2);
        memcpy(outputBuffer + 32, &d2sBinaryByteCount, 2);
        memcpy(outputBuffer + 34, &statCount, 2);

        // at (statsArray + index*8) , 2 bytes statLayer(param), 2 bytes statId , 4 bytes statValue
        if (statCount > 0) {
            memcpy(outputBuffer + 36, statsArray, 8 * statCount);
        }
        uint16_t nextOffset = 36 + 8 * statCount;

        memcpy(outputBuffer + nextOffset, asciiItemName, itemNameLength);
        nextOffset += itemNameLength;

        memcpy(outputBuffer + nextOffset, asciiItemAttributes, itemDescriptionLength);
        nextOffset += itemDescriptionLength;

        memcpy(outputBuffer + nextOffset, itemD2SBinary, d2sBinaryByteCount);

        D2Game_destroyUnit(pGame, pUnit);
        
        outputBuffer += totalMessageLength;
        combinedSizeOfAllItems += totalMessageLength;

        totalItemCount++;
        if (totalItemCount % 100000 == 0) {
            LARGE_INTEGER timestamp;
            QueryPerformanceCounter(&timestamp);
            uint64_t currentNanos = (uint64_t)timestamp.QuadPart * 1000000000 / qpcFrequency.QuadPart;
            logPrintf("%llu\n", (currentNanos - previousNanos));
            previousNanos = currentNanos;
        }

    }
    newItemCount = 0;

    //validateAllMemoryFreed();
    ShortTermMemoryAllocator_resetSimpleMemoryAllocator();

    BoundedBuffer_ReleaseProducerSlot(bufferToHandoffDataToSocketWriterThread, combinedSizeOfAllItems);
}


#define JMP_SIZE 5

void saveItemPointer() {
    uint32_t tmp;
    // asm("\t movl %%edi,%0" : "=r"(edi));
    __asm {
        mov tmp, edi
    };
    /*
        if (itemEventDataSize == -1) {
            return;
        } */

    newItems[newItemCount++] = tmp;
}

/*

push arg1
push arg2
CALL func (pushes EIP)
 sp -> eip
       arg2
       arg1
pushad
 sp    -> registers
 sp+32 -> eip
          arg2
          arg1

CALL beforeFunc
 sp    -> eip2
 sp+4  -> registers
 sp+36 -> eip
          arg2
          arg1

beforeFunc finishes and pops eip2 off the stack and returns to that address to run next instruction
 sp    -> registers
 sp+32 -> eip
          arg2
          arg1

popad (now all registers have their original values from before pushad was called)
 sp -> eip
       arg2
       arg1

SAVE [SP] (contains original eip) to some location on heap
add esp, 4

 sp -> arg2
       arg1

PUSH eip + 5 + numCopiedBytes + 5 // the return address (eip + sizeof(PUSH) + numCopiedBytes + sizeof(JMP) )
// first numCopiedBytes bytes of func copied here
JMP func+numCopiedBytes

 sp -> eip3
       arg2
       arg1

func finishes , pops eip3 and any arguments off the stack

        arg2
        arg1
  sp ->        (desired sp when returning control abck to original caller)

pushad
CALL afterFunc
popad

JMP to original eip (retrieve it from the heap)




Calling code does CALL , which puts EIP onto the stack
JMP instruction jumps to trampoline function , which will pretend it was just CALLed

push ebp
move ebp, esp
pushad
CALL beforeFunc





 */

void wrapFunction(PVOID originalFunc, PVOID beforeFunc, PVOID afterFunc, DWORD lenToCopy) {
    DWORD garbage;
    byte* wrapperFunc;

    wrapperFunc = (byte*)VirtualAlloc(0, 100, MEM_COMMIT, PAGE_READWRITE);
    VirtualProtect(wrapperFunc, 100, PAGE_EXECUTE_READWRITE, &garbage);


    DWORD offsetToBeforeFunc = ((DWORD)beforeFunc - ((DWORD)wrapperFunc + 6));
    DWORD addressToStoreOriginalReturnAddress = ((DWORD)wrapperFunc + 64);
    DWORD newReturnAddressForOriginalFunc = (DWORD)wrapperFunc + 30 + lenToCopy;
    DWORD offsetToOriginalFunc = (DWORD)((DWORD)originalFunc + lenToCopy) - ((DWORD)wrapperFunc + 30 + lenToCopy);
    DWORD offsetToAfterFunc = ((DWORD)afterFunc - ((DWORD)wrapperFunc + 36 + lenToCopy));

    BYTE start[25] = {
            0x60,
            0xE8, 0x00, 0x00, 0x00, 0x00,
            0x8B, 0x4C, 0x24, 0x20,
            0x89, 0x0D, 0x00, 0x00, 0x00, 0x00,
            0x61,
            0x83, 0xC4, 0x04,
            0x68, 0x00, 0x00, 0x00, 0x00
    };

    BYTE end[18] = {
            0xE9, 0x00, 0x00, 0x00, 0x00,
            0x60,
            0xE8, 0x00, 0x00, 0x00, 0x00,
            0x61,
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00
    };

    memcpy(start + 2, &offsetToBeforeFunc, 4);
    memcpy(start + 12, &addressToStoreOriginalReturnAddress, 4);
    memcpy(start + 21, &newReturnAddressForOriginalFunc, 4);
    memcpy(end + 1, &offsetToOriginalFunc, 4);
    memcpy(end + 7, &offsetToAfterFunc, 4);
    memcpy(end + 14, &addressToStoreOriginalReturnAddress, 4);

    memcpy(wrapperFunc, start, 25);
    memcpy(wrapperFunc + 25, originalFunc, lenToCopy);
    memcpy(wrapperFunc + 25 + lenToCopy, end, 18);


    DWORD offsetToWrapperFunc = ((DWORD)wrapperFunc) - ((DWORD)originalFunc + 5);
    BYTE buf[20];
    memset(buf, 0x90, 20);
    buf[0] = 0xE9;
    memcpy(buf + 1, &offsetToWrapperFunc, 4);

    DWORD oldProtect;
    VirtualProtect((PBYTE)originalFunc, JMP_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(originalFunc, buf, lenToCopy);
    VirtualProtect((PBYTE)originalFunc, JMP_SIZE, oldProtect, &garbage);

}


void ReplaceFunction(PBYTE pOriginalFunc, PBYTE replacementFunc) {
    DWORD offsetToWrapperFunc = ((DWORD)replacementFunc) - ((DWORD)pOriginalFunc + 5);
    BYTE buf[5];
    buf[0] = 0xE9;
    memcpy(buf + 1, &offsetToWrapperFunc, 4);

    DWORD garbage, oldProtect;
    VirtualProtect((PBYTE)pOriginalFunc, JMP_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(pOriginalFunc, buf, 5);
    VirtualProtect((PBYTE)pOriginalFunc, JMP_SIZE, oldProtect, &garbage);
}


PBYTE DetourFunction(PBYTE pOriginalFunc, PBYTE injectedCodeToCall, DWORD lenToCopy) {
    DWORD garbage, offsetToTrampoline, offsetToInjectedFunc, offsetToOriginalFunc, oldProtect;
    byte* trampoline;
    BYTE buf[20];


    //Allocate memory for trampoline function (10 bytes would've been enough,...) and make it executable
    trampoline = (byte*)VirtualAlloc(0, 24, MEM_COMMIT, PAGE_READWRITE);
    VirtualProtect(trampoline, 24, PAGE_EXECUTE_READWRITE, &garbage);

    //Copy the first bytes of the real function into trampoline
    memcpy(trampoline, pOriginalFunc, lenToCopy);

    //Prepare jump instruction to jump from inside trampoline to the real function + 5 bytes we have overwritten (they are executed inside the trampoline function)
    offsetToOriginalFunc = ((DWORD)(pOriginalFunc + lenToCopy) - (DWORD)((PBYTE)trampoline + 12 + lenToCopy));
    offsetToTrampoline = ((DWORD)(trampoline)) - ((DWORD)(pOriginalFunc + 5));
    offsetToInjectedFunc = ((DWORD)injectedCodeToCall - (DWORD)(trampoline + 1 + 5));

    // trampoline
    // (1 byte) pushad
    // (5 bytes) first 5 bytes , call to injectedCodeToCall
    // (1 byte) popad
    // (lenToCopy bytes) first lenToCopy bytes of originalFunc
    // (5 bytes) jmp back to originalFunc + lenToCopy
    // nop

    buf[0] = 0x60; // push all registers
    memcpy((PBYTE)trampoline, buf, 1);

    buf[0] = 0xE8; // CALL instruction
    memcpy(buf + 1, &offsetToInjectedFunc, 4);
    memcpy((PBYTE)trampoline + 1, buf, JMP_SIZE);

    buf[0] = 0x61; // pop all registers
    memcpy((PBYTE)trampoline + 6, buf, 1);

    memcpy(trampoline + 7, pOriginalFunc, lenToCopy);

    buf[0] = 0xE9;
    memcpy(buf + 1, &offsetToOriginalFunc, 4);
    memcpy((PBYTE)trampoline + 7 + lenToCopy, buf, JMP_SIZE);

    buf[0] = 0x90;
    memcpy((PBYTE)trampoline + 12 + lenToCopy, buf, 1);



    buf[0] = 0xE9;
    int i;
    for (i = 1; i < 20; i++) {
        buf[i] = 0x90;
    }
    memcpy(buf + 1, &offsetToTrampoline, 4);

    //Allow writing to memory of the real function
    VirtualProtect((PBYTE)pOriginalFunc, JMP_SIZE, PAGE_EXECUTE_READWRITE, &oldProtect);

    //Copy our jump instruction into the original function
    memcpy(&pOriginalFunc[0], buf, 5);

    //Restore old protection
    VirtualProtect((PBYTE)pOriginalFunc, JMP_SIZE, oldProtect, &garbage);

    return NULL;
}

// just memset, but also makes the memory writable first
void setGameMemory(void* address, int value, size_t numBytes) {
    DWORD oldProtect, garbage;
    VirtualProtect((PBYTE)address, numBytes, PAGE_EXECUTE_READWRITE, &oldProtect);
    memset(address, value, numBytes);
    VirtualProtect((PBYTE)address, numBytes, oldProtect, &garbage);
}

void memcpyAndTakeCareOfWritePermissions(void* address, void* source, size_t size) {
    DWORD oldProtect, garbage;
    VirtualProtect((PBYTE)address, size, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(address, source, size);
    VirtualProtect((PBYTE)address, size, oldProtect, &garbage);
}

void* AddressOf_UpdateItemCollision;
void* AddressOf_UpdateUniquesFoundInGame;

void callTreasureClassDropperInLoop() {
    uint32_t tmp;
    // asm("\t movl %%ebp,%0" : "=r"(ebp));
    __asm {
        mov tmp, ebp
    };
    uint32_t ebp = tmp;

    void* pGame = (void*)*(((uint32_t*)(ebp + 0x8)));
    void* pTarget = (void*)*(((uint32_t*)(ebp + 0xC)));
    void* pSource = (void*)*(((uint32_t*)(ebp + 0x28)));
    void* pTC = (void*)*(((uint32_t*)(ebp + 0x2C)));
    DWORD presetQuality = *((DWORD*)(ebp + 0x30));
    DWORD level = *((DWORD*)(ebp + 0x34));
    BOOL ignoreNoDrop = *((BOOL*)(ebp + 0x38));
    void* ppOutput = (void*)*(((uint32_t*)(ebp + 0x3C)));
    DWORD* pCounter; // =    (DWORD*) *(((uint32_t *) (ebp + 0x40)));
    DWORD maxItems = *((DWORD*)(ebp + 0x44));

    // make the items that get dropped not have any collisions (so they can all drop in the same spot)
    setGameMemory(AddressOf_UpdateItemCollision, 0x90, 3);
    // prevent it from saving the unique item to the list of unique items found so far in current game
    setGameMemory(AddressOf_UpdateUniquesFoundInGame, 0x90, 7);

    // dont call Game_GetMaxOfPlayersNAndLivingPlayerCount_00535790
    // instead just assume it will return 1
    byte patch1[5] = { 0xB8, 0x01, 0x00, 0x00, 0x00 }; // mov eax, 1
    memcpyAndTakeCareOfWritePermissions(
        reinterpret_cast<PVOID>(GetDllOffset("D2Client.dll", 0x15A8F4)), &patch1, 5); // 0x55A8F4
    
    ReplaceFunction(
        reinterpret_cast<PBYTE>(GetDllOffset("D2Client.dll", 0x00A080)), // 0x40A080
        reinterpret_cast<PBYTE>((DWORD)ShortTermMemoryAllocator_Malloc));

    ReplaceFunction(
        reinterpret_cast<PBYTE>(GetDllOffset("D2Client.dll", 0x00A1F0)), // 0x40A1F0
        reinterpret_cast<PBYTE>((DWORD)ShortTermMemoryAllocator_Realloc));

    ReplaceFunction(
        reinterpret_cast<PBYTE>(GetDllOffset("D2Client.dll", 0x009AB0)), // 0x409AB0
        reinterpret_cast<PBYTE>((DWORD)ShortTermMemoryAllocator_Free));

    logPrintf("Finished patching simpleMalloc and SimpleFree \n");

    ShortTermMemoryAllocator_SleepAllThreadsExceptThisOne = GetCurrentThreadId();
    logPrintf("Main Thread ID : %d \n", ShortTermMemoryAllocator_SleepAllThreadsExceptThisOne);
    
    sgptDataTable = (byte*)0x744304;
    pItemTypesTxtTable = (byte*)((*((uint32_t*)sgptDataTable)) + 3064);
    itemTypesTxtTable = (byte*)(*((uint32_t*)pItemTypesTxtTable));

    uint32_t* pGameAs4ByteIntArray = (uint32_t*)pGame;
    DWORD oldProtect, garbage;

    for (uint64_t i = 0; i < 1000000000000; i++) {

        if (pGameAs4ByteIntArray[40] > 4000000000) { // don't let the next item unitId counter overflow 2^32
            VirtualProtect(((uint8_t*) pGame) + 0xA0, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
            pGameAs4ByteIntArray[40] = 1000000000;
            VirtualProtect(((uint8_t*) pGame) + 0xA0, 4, oldProtect, &garbage);
        }



        //memset(ppOutput, 0, 40000);


        //fprintf(logFile, "Blah1 \n");
        //fflush(logFile);
        DWORD counter = (DWORD)0;
        //*pCounter = (DWORD) 0;
      //  startItemEvent(1);
        //fprintf(logFile, "Calling D2Game_tcDropper \n");
        //fflush(logFile);

        newItemCount = 0;
        D2Game_tcDropper(pGame, pTarget, pSource, pTC, presetQuality, level, ignoreNoDrop, ppOutput, &counter, maxItems);
        //fprintf(logFile, "Blah2 \n");
        //fflush(logFile);
        //fprintf(logFile, "Iteration %d , itemCount= %d \n", i, *((uint8_t*)(&counter)));
        //fflush(logFile);

        //long long int timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        //fprintf(outputFile, "X%llu %s\n", timeMs, hexStr((unsigned char *) ppOutput, 40).c_str());

       //fprintf(logFile, "Blah3 \n");
        //fflush(logFile);

        //fprintf(logFile, "Calling finishAndWriteItemEvent \n");
        //fflush(logFile);

        finishAndWriteItemEvent(pGame ,i);

        //fprintf(logFile, "Found items : %d \n", (uint32_t) (counter) );
        //fprintf(logFile,"%d\n", i);

    }

}




PatchHook Patches[] = {
        { GetDllOffset("D2Client.dll", 0x157E46), (DWORD)saveItemPointer, 6},
        { GetDllOffset("D2Client.dll", 0x15B015), (DWORD)callTreasureClassDropperInLoop, 4}
        // { GetDllOffset("D2Client.dll", 0x1A6830), (DWORD) handleMonsterKill, 5}
};



void init() {

    sgptDataTable = (byte*)0x744304;
    pItemTypesTxtTable = (byte*)((*((uint32_t*)sgptDataTable)) + 3064);
    itemTypesTxtTable = (byte*)(*((uint32_t*)pItemTypesTxtTable));
    
    Logging_init("C:\\Users\\sully\\d2log_log.txt");


    QueryPerformanceFrequency(&qpcFrequency);


    bufferToHandoffDataToSocketWriterThread = BoundedBuffer_create(10, 20000);

    CreateThread(NULL, 0, socketWriterThreadEntryPoint, NULL, 0, NULL);


    //itemDroppingEventData = new byte[32768];
    //itemEventDataSize = -1;

    newItems = new uint32_t[100];
    newItemCount = 0;

    

    for (int i = 0; i < ArraySize(Patches); i++)
    {
        DetourFunction(
            reinterpret_cast<PBYTE>(Patches[i].originalAddress),
            reinterpret_cast<PBYTE>(Patches[i].addressOfNewCodeToRun),
            Patches[i].lengthOfInstructionsToOverwrite);

        //fprintf( logFile, "Setup detour %i\n", i);


    }

    // { GetDllOffset("D2Client.dll", 0x1A6830), (DWORD) handleMonsterKill, 5}

    D2GameFunctions_init();

    AddressOf_UpdateItemCollision = (void*)GetDllOffset("D2Client.dll", 0x24CB26);
    AddressOf_UpdateUniquesFoundInGame = (void*)GetDllOffset("D2Client.dll", 0x1565B6);

    /*
    wrapFunction(
            (PVOID) GetDllOffset("D2Client.dll", 0x1A6830) ,
            (PVOID) beforeMonsterKill,
            (PVOID) afterMonsterKill,
            5);

    wrapFunction(
            (PVOID) GetDllOffset("D2Client.dll", 0x184420) ,
            (PVOID) beforeObjectUse,
            (PVOID) afterObjectUse,
            6);

*/

}

void cleanup() {
    Logging_close();
}



bool __stdcall DllMain(HMODULE /*module*/, DWORD reason, LPVOID /*reserved*/) {
    //if (reason == DLL_PROCESS_ATTACH) init();
    //if (reason == DLL_PROCESS_DETACH) cleanup();
    return true;
}





