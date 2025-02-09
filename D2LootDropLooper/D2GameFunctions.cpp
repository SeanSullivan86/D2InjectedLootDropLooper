#include <windows.h>
#include "D2GameFunctions.h"

DWORD GetDllOffset(const char* DllName, int Offset) {
    HMODULE hMod = GetModuleHandle(NULL);

    if (Offset < 0)
        return (DWORD)GetProcAddress(hMod, (LPCSTR)(-Offset));

    return ((DWORD)hMod) + Offset;
}

DestroyUnit* D2Game_destroyUnit;
TCDropper* D2Game_tcDropper;
GetItemAttributes* D2Game_getItemAttributes;
GetItemName* D2Game_getItemName;
GetUnitStat* D2Game_getUnitStat;
GetItemCode* D2Game_getItemCode;
ItemTxt_GetById_006335f0* D2Game_ItemTxt_GetById_006335f0;
SerializeItemToByteBuffer* D2Game_serializeItemToByteBuffer;
Unit_FindStatListByStateAndFlags* D2Game_Unit_FindStatListByStateAndFlags;


void D2GameFunctions_init() {
    D2Game_destroyUnit = (DestroyUnit*)GetDllOffset("D2Client.dll", 0x155600);
    D2Game_tcDropper = (TCDropper*)GetDllOffset("D2Client.dll", 0x15A6D0);
    D2Game_getItemName = (GetItemName*)GetDllOffset("D2Client.dll", 0x08C060);
    D2Game_getItemAttributes = (GetItemAttributes*)GetDllOffset("D2Client.dll", 0x0E6410);
    D2Game_getUnitStat = (GetUnitStat*)GetDllOffset("D2Client.dll", 0x225480);
    D2Game_getItemCode = (GetItemCode*)GetDllOffset("D2Client.dll", 0x228590);
    D2Game_ItemTxt_GetById_006335f0 = (ItemTxt_GetById_006335f0*)GetDllOffset("D2Client.dll", 0x2335F0);
    D2Game_serializeItemToByteBuffer = (SerializeItemToByteBuffer*)GetDllOffset("D2Client.dll", 0x2313E0);
    D2Game_Unit_FindStatListByStateAndFlags = (Unit_FindStatListByStateAndFlags*)GetDllOffset("D2Client.dll", 0x2257D0);
}