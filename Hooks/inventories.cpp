#include "inventories.h"
#include "../dllmain.h"
#include "../MinHook.h"
#include <random>

std::random_device rng;
std::mt19937 gen(rng());

bool g_everyoneHasKnife = false;
bool g_loadoutPresetsEnabled = false;
uint32_t g_selectedLoadoutPresetIndex = 0;
uint8_t __fastcall InventoryAssign_Detour(void* thisPtr, void* /*unknown register*/, uint32_t weaponId, int quantity, int reason, int ammo)
{
#ifdef DEBUG_LOGGING
    //printf("inventory: 0x%p, weaponId=0x%X, quantity=%d, reason=%d, ammo=%d\n", thisPtr, weaponId, quantity, reason, ammo);
#endif
    if (g_loadoutPresetsEnabled)
    {
        const bool pistols = weaponId == Luger || weaponId == P_38;
        const bool snipers = weaponId == Gewehr43 || weaponId == Mosin91 || weaponId == SVT_40;
        const bool machineGuns = weaponId == PPSH || weaponId == MP_40 || weaponId == MG42 || weaponId == DP_28;
        const bool throwables = weaponId == StickGrenade || weaponId == FragGrenade || weaponId == TnT;
        const bool bazookas = weaponId == Panzerschreck || weaponId == Panzerfaust;

        void* playerObject = inventoryToPlayer[thisPtr];
        uint32_t playerTeam = 0;

        if (playerObject)
            playerTeam = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(playerObject) + 0x8);

        if (g_selectedLoadoutPresetIndex == 0) // Sniper only
        {
            if (pistols || machineGuns || bazookas || throwables)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 1) // Machine gun only
        {
            if (pistols)
            {
                weaponId = 0x0;

                std::vector<uint8_t> machineGunsPool = { PPSH, MP_40, MG42, DP_28 };
                if (playerTeam == 1)
                    machineGunsPool = { PPSH, MP_40, MG42, DP_28 };
                if (playerTeam == 2)
                    machineGunsPool = { MP_40, MG42 };
                if (playerTeam == 3)
                    machineGunsPool = { PPSH, DP_28 };

                std::uniform_int_distribution<size_t> dist(0, std::size(machineGunsPool) - 1);
                const uint32_t randomized = machineGunsPool[dist(gen)];

                if (randomized == PPSH)
                {
                    inventoryAssign(thisPtr, PPSHAmmo, 284, -1, 0);
                    inventoryAssign(thisPtr, PPSH, 1, -1, 71);
                }
                if (randomized == MP_40)
                {
                    inventoryAssign(thisPtr, MP_40Ammo, 128, -1, 0);
                    inventoryAssign(thisPtr, MP_40, 1, -1, 32);
                }
                if (randomized == MG42)
                {
                    inventoryAssign(thisPtr, MG42Ammo, 150, -1, 0);
                    inventoryAssign(thisPtr, MG42, 1, -1, 50);
                }
                if (randomized == DP_28)
                {
                    inventoryAssign(thisPtr, DP_28Ammo, 188, -1, 0);
                    inventoryAssign(thisPtr, DP_28, 1, -1, 47);
                }
            }

            if (snipers || bazookas || throwables)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 2) // Pistol only
        {
            if (snipers || machineGuns || bazookas || throwables)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 3) // Grenades only
        {
            if (pistols || snipers || machineGuns || bazookas)
                weaponId = 0x0;

            if (weaponId == FragGrenade)
            {
                quantity = 7;
                inventoryAssign(thisPtr, StickGrenade, 7, -1, 0);
                inventoryAssign(thisPtr, MedKit, 1, -1, 0);
            }

            if (weaponId == TnT)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 4) // No explosives
        {
            if (bazookas || throwables)
                weaponId = 0x0;
        }
    }

    if (weaponId == Binoculars && g_everyoneHasKnife)
        inventoryAssign(thisPtr, Knife, 1, -1, 0);

    return inventoryAssign(thisPtr, weaponId, quantity, reason, ammo);
}

void hookInvs()
{
    MH_CreateHook(reinterpret_cast<void*>(InventoryAssignAddr),
        InventoryAssign_Detour, reinterpret_cast<void**>(&inventoryAssign));
}

/*
static void PrintInventory()
{
    if (!TargetInventory)
    {
        printf("Target inventory not specified.\n");
        return;
    }

    printf("--- Dumping inventory (0x%p) ---\n", TargetInventory);

    for (int offset = 6; offset < 84; offset += 2)
    {
        uint16_t quantity = *reinterpret_cast<uint16_t*>(itemArrayBase + offset);
        if (quantity != 0)
        {
            int itemId = (offset - 6) / 2 + 1;
            printf("Item ID: 0x%X -> Quantity: %d\n", itemId, quantity);
        }
    }
    printf("--- End of Inventory Dump ---\n");
}
*/