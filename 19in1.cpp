#include "19in1.h"
#include <windows.h>
#include <string.h>
#include <stdint.h>

bool unlockMaps = false;
static bool initiallyDisabledMaps[MpMapRecordCount] = {};
static bool mapsInitialized = false;
void listMaps()
{
    uint8_t* base = reinterpret_cast<uint8_t*>(MpMapTableBaseAddr);

    DWORD oldProtect = 0;
    if (!VirtualProtect(base, MpMapRecordSize * MpMapRecordCount, PAGE_EXECUTE_READWRITE, &oldProtect))
        return;

    if (!mapsInitialized)
    {
        for (size_t i = 0; i < MpMapRecordCount; ++i)
        {
            uint32_t* flags = reinterpret_cast<uint32_t*>(base + i * MpMapRecordSize + MpMapRecordFlagsOffset);
            initiallyDisabledMaps[i] = (*flags & 0xFu) == 0x0u;
        }
        mapsInitialized = true;
    }

    if (mapsInitialized)
        for (size_t i = 0; i < MpMapRecordCount; ++i)
        {
            if (!initiallyDisabledMaps[i])
                continue;

            const char* const name = *reinterpret_cast<const char* const*>(base + i * MpMapRecordSize);
            if (
                strcmp(name, "mp_00") == 0
                || strcmp(name, "mp_02c") == 0
                || strcmp(name, "mp_03e") == 0
                || strncmp(name, "mp_07", 5) == 0
                || strcmp(name, "mp_08c") == 0
                )
                continue;

            uint32_t* flags = reinterpret_cast<uint32_t*>(base + i * MpMapRecordSize + MpMapRecordFlagsOffset);
            if (unlockMaps)
                *flags = (*flags & 0xFFFFFFF0u) | 0xFu;
            else
                *flags = (*flags & 0xFFFFFFF0u) | 0x0u;
        }

    VirtualProtect(base, MpMapRecordSize * MpMapRecordCount, oldProtect, &oldProtect);
}