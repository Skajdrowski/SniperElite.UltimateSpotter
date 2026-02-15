#include "dllmain.h"
#include "dxHook.h"
#include "MinHook.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_set>

#pragma comment(lib, "ws2_32.lib")
//#define LAN

#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

bool isHost = false;

static uint32_t __cdecl DirectInput_Detour()
{
    if (GUI::isVisible)
        return 0;

    return directInput();
}

#ifdef LAN
using BindFn = int (WSAAPI*)(SOCKET, const sockaddr*, int);
static BindFn WS2bind = nullptr;

static int WSAAPI bind_Detour(SOCKET s, const sockaddr* name, int namelen)
{
    if (name && namelen >= (int)sizeof(sockaddr_in) && name->sa_family == AF_INET)
        const sockaddr_in* in = reinterpret_cast<const sockaddr_in*>(name);

    const int rc = WS2bind(s, name, namelen);
    if (rc != SOCKET_ERROR)
    {
        if (name && namelen >= (int)sizeof(sockaddr_in) && name->sa_family == AF_INET)
        {
            sockaddr_in actual{};
            int actualLen = sizeof(actual);
        }
        return rc;
    }

    if (!name || namelen < (int)sizeof(sockaddr_in) || name->sa_family != AF_INET)
        return rc;

    const int err = WSAGetLastError();
    if (err != WSAEADDRINUSE)
        return rc;

    sockaddr_in tmp = *reinterpret_cast<const sockaddr_in*>(name);
    if (tmp.sin_port == 0)
        return rc;

    tmp.sin_port = 0;
    const int rc2 = WS2bind(s, reinterpret_cast<const sockaddr*>(&tmp), sizeof(tmp));
    if (rc2 != SOCKET_ERROR)
    {
        sockaddr_in actual{};
        int actualLen = sizeof(actual);
    }

    return rc2;
}
#endif

static void* __fastcall PlayerConstructor_Detour(void* thisPtr, void* /*unknown register*/, int uID, int a3, int a4)
{
    void* playerObject = playerConstructor(thisPtr, uID, a3, a4);
    void* ThisPtr = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(playerObject) + 0x1E8);

    g_uIdToInventoryMap[uID] = ThisPtr;
    return playerObject;
}

char ipBuffer[INET_ADDRSTRLEN];
static std::unordered_set<std::string> g_bannedIpAddresses;
static uint32_t __cdecl PlayerIPListener_Detour(void* netManager, void** thisPtr, uint32_t ip, uint16_t port)
{
    std::map<std::wstring, PlayerFetchEntry>::iterator it = g_playerDirectory.begin();
    if (it != g_playerDirectory.end())
    {
        PlayerFetchEntry& matchedEntry = it->second;

        in_addr addr{};
        addr.s_addr = ip;
        inet_ntop(AF_INET, &addr, ipBuffer, INET_ADDRSTRLEN); // Converts to dotted-decimal format
    }
    return playerIPListener(netManager, thisPtr, ip, port);
}
bool banIpAddress(const std::string& ipAddress)
{
    if (ipAddress.empty() || ipAddress == "localhost")
        return false;

    const auto [_, inserted] = g_bannedIpAddresses.insert(ipAddress);
    return inserted;
}
bool unBanIpAddress(const std::string& ipAddress)
{
    if (!g_bannedIpAddresses.contains(ipAddress))
        return false;

    return g_bannedIpAddresses.erase(ipAddress);
}
bool isIpAddressBanned(const std::string& ipAddress)
{
    if (ipAddress.empty())
        return false;

    return g_bannedIpAddresses.count(ipAddress) > 0;
}

std::map<uint32_t, void*> g_uIdToInventoryMap;
std::map<std::wstring, PlayerFetchEntry> g_playerDirectory;
bool isPopulated = false;
wchar_t greetBuffer[36];
static uint32_t __cdecl PlayerFetch_Detour(Fetch* fetchStruct)
{
    isPopulated = (fetchStruct->uID > 0);
    void* inventoryPtr = g_uIdToInventoryMap.count(fetchStruct->uID) ? g_uIdToInventoryMap[fetchStruct->uID] : nullptr;

    const std::wstring nameKey = fetchStruct->nickname;
    PlayerFetchEntry& entry = g_playerDirectory[nameKey];

    entry.uid = fetchStruct->uID;
    entry.inventory = inventoryPtr;
    entry.isOnline = fetchStruct->isJoining != 0;
    entry.displayName = nameKey;

    if (fetchStruct->uID == 1000 && isHost)
    {
        wsprintfW(greetBuffer, L"Hello, %s :-)", fetchStruct->nickname);
        entry.ipAddress = "localhost";
    }
    else
        entry.ipAddress = ipBuffer;

    if (fetchStruct->isJoining && isIpAddressBanned(entry.ipAddress))
        kick(fetchStruct->uID, 4);

    if (!fetchStruct->isJoining)
        g_uIdToInventoryMap.erase(fetchStruct->uID);

    return playerFetch(fetchStruct);
}

bool g_everyoneHasKnife = false;
bool g_loadoutPresetsEnabled = false;
uint32_t g_selectedLoadoutPresetIndex = 0;
static uint8_t __fastcall InventoryAssign_Detour(void* thisPtr, void* /*unknown register*/, uint32_t weaponId, int quantity, int reason, int ammo)
{
    //printf("inventory: 0x%p, weaponId=0x%X, quantity=%d, reason=%d, ammo=%d\n", thisPtr, weaponId, quantity, reason, ammo);

    if (g_loadoutPresetsEnabled)
    {
        if (g_selectedLoadoutPresetIndex == 0) // Sniper only
        {
            if (weaponId == 0x16 || weaponId == 0x17)
                weaponId = 0x0;

            if (weaponId == 0x18 || weaponId == 0x19 || weaponId == 0x1A || weaponId == 0x1B)
                weaponId = 0x0;

            if (weaponId == 0x1D)
                weaponId = 0x0;

            if (weaponId == 0x9 || weaponId == 0xA || weaponId == 0xF)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 1) // Machine gun only
        {
            if (weaponId == 0x16 || weaponId == 0x17)
            {
                weaponId = 0x0;

                srand(static_cast<uint32_t>(time(nullptr)));
                const uint32_t machineGuns[] = {
                    0x18, 0x18, 0x18,
                    0x19, 0x19, 0x19,
                    0x1A, // make MG40 a bit rare, cuz it's OP ?
                    0x1B, 0x1B, 0x1B
                };
                int randomized = machineGuns[rand() % (sizeof(machineGuns) / sizeof(*machineGuns))];

                if (randomized == 0x18)
                {
                    inventoryAssign(thisPtr, 0x3, 284, -1, 0);
                    inventoryAssign(thisPtr, 0x18, 1, -1, 71);
                }
                if (randomized == 0x19)
                {
                    inventoryAssign(thisPtr, 0x4, 128, -1, 0);
                    inventoryAssign(thisPtr, 0x19, 1, -1, 32);
                }
                if (randomized == 0x1A)
                {
                    inventoryAssign(thisPtr, 0x5, 150, -1, 0);
                    inventoryAssign(thisPtr, 0x1A, 1, -1, 50);
                }
                if (randomized == 0x1B)
                {
                    inventoryAssign(thisPtr, 0x6, 188, -1, 0);
                    inventoryAssign(thisPtr, 0x1B, 1, -1, 47);
                }
            }

            if (weaponId == 0x13 || weaponId == 0x14 || weaponId == 0x15)
                weaponId = 0x0;

            if (weaponId == 0x1D)
                weaponId = 0x0;

            if (weaponId == 0x9 || weaponId == 0xA || weaponId == 0xF)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 2) // Pistol only
        {
            if (weaponId == 0x13 || weaponId == 0x14 || weaponId == 0x15)
                weaponId = 0x0;

            if (weaponId == 0x18 || weaponId == 0x19 || weaponId == 0x1A || weaponId == 0x1B)
                weaponId = 0x0;

            if (weaponId == 0x1D)
                weaponId = 0x0;

            if (weaponId == 0x9 || weaponId == 0xA || weaponId == 0xF)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 3) // Grenades only
        {
            if (weaponId == 0x16 || weaponId == 0x17)
                weaponId = 0x0;

            if (weaponId == 0x13 || weaponId == 0x14 || weaponId == 0x15)
                weaponId = 0x0;

            if (weaponId == 0x18 || weaponId == 0x19 || weaponId == 0x1A || weaponId == 0x1B)
                weaponId = 0x0;

            if (weaponId == 0x1D)
                weaponId = 0x0;

            if (weaponId == 0xA)
            {
                quantity = 50;
                inventoryAssign(thisPtr, 0x9, 50, -1, 0);
                inventoryAssign(thisPtr, 0xD, 1, -1, 0);
            }

            if (weaponId == 0xF)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 4) // No explosives
        {
            if (weaponId == 0x1D)
                weaponId = 0x0;

            if (weaponId == 0x9 || weaponId == 0xA || weaponId == 0xF)
                weaponId = 0x0;
        }
    }

    if (weaponId == 0x11 && g_everyoneHasKnife)
        inventoryAssign(thisPtr, 0xC, 1, -1, 0);

    return inventoryAssign(thisPtr, weaponId, quantity, reason, ammo);
}

static uint32_t __cdecl InventoryCap_Detour(uint32_t weaponId)
{
    if ((weaponId == 0x9 || weaponId == 0xA) && g_loadoutPresetsEnabled && g_selectedLoadoutPresetIndex == 3)
        return 50;

    return inventoryCap(weaponId);
}

static void __fastcall AutoBalanceUpdate_Detour(void* funcPtr, void* /*edx*/)
{
    if (!balanceToggle)
        return autoBalanceUpdate(funcPtr);

    return;
}

static std::vector<Coords> customSpawns{};
static std::atomic<uint32_t> customSpawnIndex{};
static std::vector<void*> injectedSpawnPoints;
static void* __fastcall SpawnPointInit_Detour(void* self, void* /*edx*/, int a2, int** a3)
{
    if (strcmp(curLevel, "01b.pc") == 0)
    {
        customSpawns = {
            { 107.83f, -6.45f, -89.65f, russia, crouch },
            { 257.8721008f, 9.7f, -137.5167542f, germany, crouch }
        };
    }
    if (strcmp(curLevel, "04a.pc") == 0)
    {
        customSpawns = {
            { -55.64f, -15.5f, -71.16f, russia, prone },
            { 76.52f, -15.2f, -80.22f, germany, prone }
        };
    }

    static bool isInHook = false;
    if (isInHook)
        return spawnPointInit(self, a2, a3);

    isInHook = true;

    void* sp = spawnPointInit(self, a2, a3);
    if (!sp)
    {
        isInHook = false;
        return sp;
    }

    if (!customSpawns.empty())
    {
        uint32_t origuID = *(uint32_t*)((char*)sp + spawnPointOffset);
        static std::atomic<uint32_t> injecteduID{ 0x06900000u };

        static std::atomic<bool> s_seeded{ false };
        bool expected = false;
        if (s_seeded.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        {
            uint32_t seed = 0x6900000u | (origuID & 0xFFFFFu);
            if (seed == 0x6900000u)
                seed = 0x6900001u;

            injecteduID.store(seed, std::memory_order_relaxed);
        }

        auto emitSpawn = [&](const Coords entry, uint32_t modeMask, uint32_t teamMask)
            {
                void* newSpawn = operatorNew(spawnPointSize);
                if (!newSpawn)
                    return;

                memcpy(newSpawn, sp, spawnPointSize);

                uint32_t newuID =
                    injecteduID.fetch_add(1, std::memory_order_relaxed) + 1;

                *(uint32_t*)((char*)newSpawn + spawnPointOffset) = newuID;
                *(float*)((char*)newSpawn + spawnPosXOffset) = entry.x;
                *(float*)((char*)newSpawn + spawnPosYOffset) = entry.y;
                *(float*)((char*)newSpawn + spawnPosZOffset) = entry.z;
                *(uint32_t*)((char*)newSpawn + spawnPostureOffset) = entry.posture;

                *(uint32_t*)((char*)newSpawn + spawnTeamOffset) = teamMask;
                *(uint32_t*)((char*)newSpawn + spawnGameModeOffset) = modeMask;
                /*
                printf("[SPAWN] new uid=0x%08X teamMask=0x%X pos=(%.3f, %.3f, %.3f) clone=0x%p template=0x%p\n",
                    newuID, teamMask,
                    entry.x, entry.y, entry.z,
                    newSpawn, sp);
                */
                spawnPointInject(spawnListTable, newSpawn);
                injectedSpawnPoints.push_back(newSpawn);
            };

        uint32_t idx =
            customSpawnIndex.fetch_add(1, std::memory_order_relaxed);

        if (idx < customSpawns.size())
        {
            const Coords& entry = customSpawns[idx];
            emitSpawn(entry, 0x8, 0x1);
            emitSpawn(entry, 0x10, entry.teamMask);
        }
    }

    isInHook = false;
    return sp;
}

static void* __fastcall SpawnPointErase_Detour(void* self, void* /*edx*/, uint8_t flags)
{
    static bool s_inCleanup = false;
    if (s_inCleanup)
        return spawnPointErase(self, flags);

    if (!injectedSpawnPoints.empty())
    {
        s_inCleanup = true;
        std::vector<void*> toDelete;
        toDelete.swap(injectedSpawnPoints);

        for (void* sp : toDelete)
        {
            if (!sp)
                continue;

            if (sp == self)
                continue;

            spawnPointErase(sp, 1);
        }

        customSpawnIndex.store(0, std::memory_order_relaxed);
        customSpawns.clear();
        s_inCleanup = false;
    }

    return spawnPointErase(self, flags);
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

static void Thread()
{
    while (true)
    {
        /*
        if (GetAsyncKeyState(VK_F7) & 1)
        {
            PrintInventory();
            Sleep(1000);
        }
        */
        isHost = *((const uint8_t*)0x7814F5);
        curLevel = (const char*)0x8185A0;
        if (*((const char*)0x818AB0) == *"frontsc2.dds" && isPopulated)
        {
            isPopulated = false;
            g_playerDirectory.clear();
            g_uIdToInventoryMap.clear();
            wsprintfW(greetBuffer, L"Host currently not in-game");
        }

        Sleep(1);
    }
}

static void Init()
{
    /*
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    */

    if (MH_Initialize() != MH_OK)
        return;
#ifdef LAN
    if (MH_CreateHookApi(L"WSOCK32.dll", "bind", reinterpret_cast<void*>(&bind_Detour), reinterpret_cast<void**>(&WS2bind)) != MH_OK)
        return;
#endif
    if (MH_CreateHook(reinterpret_cast<void*>(DirectInputAddr),
        DirectInput_Detour,
        reinterpret_cast<void**>(&directInput)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(PlayerConstructorAddr),
        PlayerConstructor_Detour,
        reinterpret_cast<void**>(&playerConstructor)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(PlayerIPListenerAddr),
        PlayerIPListener_Detour,
        reinterpret_cast<void**>(&playerIPListener)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(PlayerFetchAddr),
        PlayerFetch_Detour,
        reinterpret_cast<void**>(&playerFetch)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(InventoryAssignAddr),
        InventoryAssign_Detour,
        reinterpret_cast<void**>(&inventoryAssign)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(InventoryCapAddr),
        InventoryCap_Detour,
        reinterpret_cast<void**>(&inventoryCap)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(AutoBalanceUpdateAddr),
        AutoBalanceUpdate_Detour,
        reinterpret_cast<void**>(&autoBalanceUpdate)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(spawnPointInitAddr),
        SpawnPointInit_Detour,
        reinterpret_cast<void**>(&spawnPointInit)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(spawnPointEraseAddr),
        SpawnPointErase_Detour,
        reinterpret_cast<void**>(&spawnPointErase)) != MH_OK)
        return;

    MH_EnableHook(MH_ALL_HOOKS);
    InstallDirect3DHook();
    CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(Thread), nullptr, 0, nullptr);
    wsprintfW(greetBuffer, L"Host currently not in-game");
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init();
    return TRUE;
}