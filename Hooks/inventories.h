#pragma once
#include <unordered_map>

enum item : uint8_t
{
    PistolAmmo = 0x1,
    RifleAmmo = 0x2,
    PPSHAmmo = 0x3,
    MP_40Ammo = 0x4,
    MG42Ammo = 0x5,
    DP_28Ammo = 0x6,
    PanzerschreckAmmo = 0x26,

    Panzerfaust = 0x8,
    Panzerschreck = 0x1D,

    StickGrenade = 0x9,
    FragGrenade = 0xA,
    SmokeGrenade = 0xB,

    TnT = 0xF,
    TimeBomb = 0x1C,
    TripWire = 0x1E,

    Knife = 0xC,

    MedKit = 0xD,
    Bandage = 0xE,

    Binoculars = 0x11,

    Gewehr43 = 0x13,
    Mosin91 = 0x14,
    SVT_40 = 0x15,

    Luger = 0x16,
    P_38 = 0x17,

    PPSH = 0x18,
    MP_40 = 0x19,

    MG42 = 0x1A,
    DP_28 = 0x1B
};

using InventoryAssignFn = uint8_t(__thiscall*)(void*, uint32_t, int, int, int);
static InventoryAssignFn inventoryAssign = nullptr;
uint8_t __fastcall InventoryAssign_Detour(void* thisPtr, void* /*unknown register*/, uint32_t weaponId, int quantity, int reason, int ammo);
extern bool g_everyoneHasKnife;
extern bool g_loadoutPresetsEnabled;
extern uint32_t g_selectedLoadoutPresetIndex;

static std::unordered_map<void*, void*> inventoryToPlayer;

extern void hookInvs();