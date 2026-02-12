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

static uint32_t __fastcall ClientHistoryInit_Detour(void* thisPtr, void* /*edx*/)
{
    for (uint32_t i = 0; i < ClientHistoryMaxClients; ++i)
    {
        ClientHistoryEntry& e = clients[i];
        memset(&e, 0, sizeof(e));
        e.playerId = 999;
        e.connected = 0;
        e.value100 = 100;
        e.state3 = 3;
    }
    return 0;
}

static uint32_t __fastcall ClientHistoryCountConnected_Detour(void* thisPtr, void* /*edx*/)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < ClientHistoryMaxClients; ++i)
        count += clients[i].connected != 0;
    return count;
}

static uint8_t* __fastcall ClientHistoryFindByuID_Detour(void* thisPtr, void* /*edx*/, uint32_t uID, uint8_t requireConnected)
{
    for (uint32_t i = 0; i < ClientHistoryMaxClients; ++i)
    {
        ClientHistoryEntry* entry = &clients[i];
        if (entry->playerId != uID)
            continue;

        if (!entry->connected && requireConnected)
            return nullptr;

        return reinterpret_cast<uint8_t*>(entry);
    }

    return nullptr;
}

static uint8_t* __fastcall ClientHistoryIterNextConnected_Detour(void* thisPtr, void* /*edx*/, uint32_t cursorPtr)
{
    uint8_t** cursor = reinterpret_cast<uint8_t**>(cursorPtr);

    uint32_t idx = 0;
    if (*cursor)
    {
        const uintptr_t base = reinterpret_cast<uintptr_t>(clients);
        const uintptr_t cur = reinterpret_cast<uintptr_t>(*cursor);
        if (cur >= base)
        {
            const uintptr_t diff = cur - base;
            idx = diff / sizeof(ClientHistoryEntry) + 1;
        }
    }

    *cursor = nullptr;
    for (; idx < ClientHistoryMaxClients; ++idx)
    {
        ClientHistoryEntry* entry = &clients[idx];
        if (!entry->connected)
            continue;
        if (entry->playerId == 999)
            continue;

        *cursor = reinterpret_cast<uint8_t*>(entry);
        return *cursor;
    }

    return nullptr;
}

static uint8_t* __fastcall ClientHistoryFindFreeSlot_Detour(void* thisPtr, void* /*edx*/)
{
    for (uint32_t i = 0; i < ClientHistoryMaxClients; ++i)
    {
        ClientHistoryEntry& e = clients[i];
        if (e.playerId == 999 || e.connected == 0)
            return reinterpret_cast<uint8_t*>(&e);
    }

    return nullptr;
}

static uint8_t __fastcall ClientHistoryAddOrUpdate_Detour(void* thisPtr, void* /*edx*/, void* entryPtr)
{
    const ClientHistoryEntry* incoming = reinterpret_cast<const ClientHistoryEntry*>(entryPtr);
    const uint32_t key = incoming->playerId;

    for (uint32_t i = 0; i < ClientHistoryMaxClients; ++i)
    {
        ClientHistoryEntry& existing = clients[i];
        if (existing.playerId == key)
        {
            if (existing.connected)
                return 0;
            memcpy(&existing, incoming, sizeof(ClientHistoryEntry));
            return 1;
        }
    }

    uint8_t* slot = ClientHistoryFindFreeSlot_Detour(thisPtr, nullptr);
    if (!slot)
        return 0;

    memcpy(slot, incoming, sizeof(ClientHistoryEntry));
    return 1;
}

static uint32_t __fastcall ClientHistoryClearByuID_Detour(void* thisPtr, void* /*edx*/, uint32_t uID)
{
    for (uint32_t i = 0; i < ClientHistoryMaxClients; ++i)
    {
        ClientHistoryEntry& e = clients[i];
        if (e.playerId != uID)
            continue;

        if (!e.connected)
            return i * sizeof(ClientHistoryEntry);

        e.connected = 0;
        memset(reinterpret_cast<uint8_t*>(&e) + 0x44, 0, 0x34);
        e.value100 = 100;
        e.state3 = 3;
        return i * sizeof(ClientHistoryEntry);
    }

    return ClientHistoryMaxClients;
}

static int32_t __fastcall ClientHistoryIndexByuID_Detour(void* thisPtr, void* /*edx*/, uint32_t uID, uint8_t requireConnected)
{
    for (uint32_t i = 0; i < ClientHistoryMaxClients; ++i)
    {
        const ClientHistoryEntry& e = clients[i];
        if (e.playerId != uID)
            continue;

        if (e.playerId == 999)
            return -1;
        if (!e.connected && requireConnected)
            return -1;

        return i;
    }
    return -1;
}

static bool PatchMemory(uintptr_t address, const void* data, size_t size)
{
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    CopyMemory(reinterpret_cast<void*>(address), data, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);

    DWORD unused = 0;
    VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &unused);
    return true;
}

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

static void __cdecl SlotsBroadcast_Detour(void* optionsBlob)
{
    if (optionsBlob)
    {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(optionsBlob) + 0x50) = 0x20;
    }
    slotsBroadcast(optionsBlob);
}

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
    if (MH_Initialize() != MH_OK)
        return;

    clients = reinterpret_cast<ClientHistoryEntry*>(VirtualAlloc(
        nullptr,
        ClientHistoryMaxClients * sizeof(ClientHistoryEntry),
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE));

    const uint8_t idleDisconnect[] = {0xEB, 0x21};
    PatchMemory(DisconnectIdleAddr, idleDisconnect, sizeof(idleDisconnect));

    const uint8_t clientHistories[] = {0x83, 0xF8, 0x20};
    PatchMemory(ClientHistoriesCapAddr, clientHistories, sizeof(clientHistories));

#ifdef LAN
    if (MH_CreateHookApi(L"WSOCK32.dll", "bind", reinterpret_cast<void*>(&bind_Detour), reinterpret_cast<void**>(&WS2bind)) != MH_OK)
        return;
#endif
    if (MH_CreateHook(reinterpret_cast<void*>(DirectInputAddr),
        DirectInput_Detour,
        reinterpret_cast<void**>(&directInput)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(ClientHistoryInitAddr),
        ClientHistoryInit_Detour,
        reinterpret_cast<void**>(&clientHistoryInit)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(ClientHistoryCountConnectedAddr),
        ClientHistoryCountConnected_Detour,
        reinterpret_cast<void**>(&clientHistoryCountConnected)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(ClientHistoryFindByNumberAddr),
        ClientHistoryFindByuID_Detour,
        reinterpret_cast<void**>(&clientHistoryFindByNumber)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(ClientHistoryIterNextConnectedAddr),
        ClientHistoryIterNextConnected_Detour,
        reinterpret_cast<void**>(&clientHistoryIterNextConnected)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(ClientHistoryFindFreeSlotAddr),
        ClientHistoryFindFreeSlot_Detour,
        reinterpret_cast<void**>(&clientHistoryFindFreeSlot)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(ClientHistoryAddOrUpdateAddr),
        ClientHistoryAddOrUpdate_Detour,
        reinterpret_cast<void**>(&clientHistoryAddOrUpdate)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(ClientHistoryClearByNumberAddr),
        ClientHistoryClearByuID_Detour,
        reinterpret_cast<void**>(&clientHistoryClearByuID)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(ClientHistoryIndexByNumberAddr),
        ClientHistoryIndexByuID_Detour,
        reinterpret_cast<void**>(&clientHistoryIndexByuID)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(SlotsBroadcastAddr),
        reinterpret_cast<void*>(&SlotsBroadcast_Detour),
        reinterpret_cast<void**>(&slotsBroadcast)) != MH_OK)
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