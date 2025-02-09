#pragma once
#include <cstdint>

typedef unsigned char byte;

typedef void __fastcall DestroyUnit(void* pGame, void* pUnit);
extern DestroyUnit* D2Game_destroyUnit;

typedef void __fastcall TCDropper(void* pGame,
    void* pTarget,
    void* pSource,
    void* pRecord,
    DWORD nPresetQuality,
    DWORD nLevel,
    BOOL bIgnoreNoDrop,
    void* ppOutput,
    DWORD* pCounter,
    DWORD nMaxItems);
extern TCDropper* D2Game_tcDropper;

typedef void __fastcall GetItemAttributes(void* pUnitItem, void* pOutput, uint32_t unknown1, uint32_t unknown2, uint32_t unknown3);
extern GetItemAttributes* D2Game_getItemAttributes;

typedef void __fastcall GetItemName(void* pUnitItem, void* pOutput, uint32_t unknown1);
extern GetItemName* D2Game_getItemName;

typedef uint32_t __stdcall GetUnitStat(void* pUnit, int statId, uint16_t nLayer);
extern GetUnitStat* D2Game_getUnitStat;

typedef uint32_t __stdcall GetItemCode(void* pUnit);
extern GetItemCode* D2Game_getItemCode;

typedef byte* __stdcall ItemTxt_GetById_006335f0(uint32_t dwClassId);
extern ItemTxt_GetById_006335f0* D2Game_ItemTxt_GetById_006335f0;

typedef uint32_t __stdcall SerializeItemToByteBuffer(void* pItemUnit, void* buffer, uint32_t maxByteSize, BOOL bServer, BOOL bSaveItemInv, BOOL bGamble);
extern SerializeItemToByteBuffer* D2Game_serializeItemToByteBuffer;

typedef byte* __stdcall Unit_FindStatListByStateAndFlags(void* pUnit, int statListStateNumber, uint32_t statListFlags);
extern Unit_FindStatListByStateAndFlags* D2Game_Unit_FindStatListByStateAndFlags;

unsigned long GetDllOffset(const char* DllName, int Offset);

void D2GameFunctions_init();