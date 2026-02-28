#include "dllmain.h"
#include "dxHook.h"
#include "MinHook.h"
#include <random>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
//#define LAN
//#define DEBUG_LOGGING

#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

std::random_device rng;
std::mt19937 gen(rng());

bool isHost = false;
const char* curLevel = nullptr;
bool customSpawns = false;
bool antiOOB = true;
bool unlockMaps = false;

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

static uint8_t __cdecl ChatCooldown_Detour(uint32_t key)
{
    const uint8_t result = chatCooldown(key);

    float* gCooldown = (float*)0x78A8EC;

    if (key == 13 && gCooldown)
        *gCooldown = 1.0f;

    return result;
}

std::map<uint32_t, void*> uIdToPlayer;
static std::unordered_map<void*, uint32_t> playerTouID;
static void* __fastcall PlayerConstructor_Detour(void* thisPtr, void* /*unknown register*/, uint32_t uID, int a3, int a4)
{
    void* playerObject = playerConstructor(thisPtr, uID, a3, a4);
#ifdef DEBUG_LOGGING
    void* playerInventory = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(playerObject) + 0x1E8);
    printf("PlayerConstructor called for uID=%d, playerObject=0x%p\n", uID, playerObject);
    printf("Extracted inventory pointer: 0x%p\n", playerInventory);
#endif

    uIdToPlayer[uID] = playerObject;
    playerTouID[playerObject] = uID;

    return playerObject;
}

static std::unordered_set<std::string> g_bannedIpAddresses;
static std::unordered_map<uint32_t, std::string> g_uIdToIp;
static void __cdecl PlayerJoinIPMap_Callback(uint32_t uID, uint32_t ip)
{
    in_addr addr{};
    addr.s_addr = ip;
    char ipStr[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);
    g_uIdToIp[uID] = ipStr;
#ifdef DEBUG_LOGGING
    printf("PlayerJoinSend: Mapped uID %d to IP %s\n", uID, ipStr);
#endif
}

static int __cdecl IPJoinSend_Detour(int connPtr, int msgPtr)
{
    if (_ReturnAddress() == reinterpret_cast<void*>(0x446CC0))
    {
        const uint32_t msgType = *reinterpret_cast<const uint32_t*>(msgPtr);
        if (msgType == 1)
        {
            uint32_t uID = 0;
            const uint32_t* data = *reinterpret_cast<uint32_t**>(msgPtr + 0x18);
            if (data)
                uID = *data;

            const uint32_t ip = *reinterpret_cast<const uint32_t*>(connPtr);
            PlayerJoinIPMap_Callback(uID, ip);
        }
    }

    return ipJoinSend(connPtr, msgPtr);
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
    return g_bannedIpAddresses.erase(ipAddress);
}
bool isIpAddressBanned(const std::string& ipAddress)
{
    return !ipAddress.empty() && g_bannedIpAddresses.count(ipAddress) > 0;
}

std::map<std::wstring, PlayerFetchEntry> g_playerDirectory;
bool isPopulated = false;
wchar_t greetBuffer[36];
static std::unordered_map<void*, playerCoords> g_entityCoords;
static std::unordered_set<void*> g_activeEntities;
static std::unordered_map<void*, void*> inventoryToPlayer;
static uint32_t __cdecl PlayerFetch_Detour(Fetch* fetchStruct)
{
    isPopulated = (fetchStruct->uID > 0);
    void* playerObject = uIdToPlayer.count(fetchStruct->uID) ? uIdToPlayer[fetchStruct->uID] : nullptr;
    void* inventoryPtr = isHost ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(playerObject) + 0x1E8) : nullptr;

    if (inventoryPtr)
        inventoryToPlayer[inventoryPtr] = playerObject;

    const std::wstring nameKey = fetchStruct->nickname;
    PlayerFetchEntry& entry = g_playerDirectory[nameKey];

    entry.uid = fetchStruct->uID;
    entry.player = playerObject;
    entry.inventory = inventoryPtr;
    entry.isOnline = fetchStruct->isJoining != 0;
    entry.displayName = nameKey;

    if (fetchStruct->uID == 1000 && isHost)
    {
        wsprintfW(greetBuffer, L"Hello, %s :-)", fetchStruct->nickname);
        entry.ipAddress = "localhost";
    }
    else
    {
        const auto it = g_uIdToIp.find(fetchStruct->uID);
        if (it != g_uIdToIp.end())
            entry.ipAddress = it->second;
    }
#ifdef DEBUG_LOGGING
    printf("PlayerFetch called for uID=%d, isJoining=%d, resolved IP=%s\n",
        fetchStruct->uID, fetchStruct->isJoining, entry.ipAddress.c_str());
#endif
    if (fetchStruct->isJoining && isIpAddressBanned(entry.ipAddress))
        kick(fetchStruct->uID, 4);

    if (fetchStruct->isJoining)
        g_activeEntities.insert(playerObject);
    else
    {
        g_activeEntities.erase(playerObject);
        uIdToPlayer.erase(fetchStruct->uID);
        g_uIdToIp.erase(fetchStruct->uID);
    }

    return playerFetch(fetchStruct);
}

const std::vector<playerCoords> karlshorstOOBs = {
    { -120.52f, -2.44f, -22.8f }, { -119.5f, -1.3f, 4.2f }, // Sewers
    { -239.8f, -2.625f, 149.85f }, { -212.4f, -1.5f, 157.f }, // Lifting barrier
    { -104.35f, -1.35f, 4.7f }, { -101.f, -0.2f, 7.f }, // A hole in the wall, near Sewers
    { -183.f, -10.55f, -41.f }, { -179.5f, -5.f, -26.6f } // Broken ceiling
};
const std::vector<playerCoords> safehouseOOBs = {
    { -55.7f, -3.85f, 71.f }, { -53.1f, 2.f, 72.1f }, // Broken window
    { -52.6f, 0.84f, 48.7f }, { -49.1f, 2.2f, 62.83f }, // Entrance's roof to the safehouse
    { -44.f, 5.2f, -33.7f }, { -34.7f, 8.1f, -29.5f } // Barrier with laying steel beam
};
const std::vector<playerCoords> missingContactOOBs = {
    { -105.f, -2.9f, -142.5f }, { -102.f, -1.2f, -140.4f }, // Lifting barrier
    { -127.f, -1.5f, 2.f }, { -126.f, -0.5f, 0.f }
};
const std::vector<playerCoords> ubahnOOBs = {
    { 30.41f, -5.4f, 5.28f }, { 35.75f, -4.12f, 9.8f }, // Neighbor room with invisible indoor walls
    { -46.2f, -1.34f, 48.f }, { -17.5f, 0.f, 80.f }, // Right side from the roof spawn near railings
    { 9.4f, -2.51f, -130.f }, { 30.9f, 0.f, -127.f } // Barrier connected with stone fence
};
const std::vector<playerCoords> holzmarktOOBs = {
    { -78.9f, -3.f, 7.8f }, { -75.5f, -0.2f, 4.9f } // Lifting barrier near sewers
};

static void __fastcall PlayerGetCoords_Detour(void* thisPtr, void* /*edx*/, float* outVec3)
{
    void* entityPtr = nullptr;
    __asm { mov entityPtr, esi }

    playerGetCoords(thisPtr, outVec3);

    if (g_activeEntities.contains(entityPtr))
    {
        const playerCoords coords{ outVec3[2], outVec3[1], outVec3[0] };
        g_entityCoords[entityPtr] = coords;

        if (outVec3[1] >= 50.0f)
        {
            const auto uID = playerTouID.find(entityPtr);
            if (uID != playerTouID.end())
                fallDamage(1337.f, uID->second);
        }

        if (antiOOB)
        {
            const std::vector<playerCoords>* activeOOB = nullptr;
            bool entered = false;

            if (strcmp(curLevel, "01a.pc") == 0)
                activeOOB = &karlshorstOOBs;
            if (strcmp(curLevel, "02a.pc") == 0)
				activeOOB = &safehouseOOBs;
            if (strcmp(curLevel, "03a.pc") == 0)
				activeOOB = &missingContactOOBs;
            if (strcmp(curLevel, "04a.pc") == 0)
                activeOOB = &ubahnOOBs;
            if (strcmp(curLevel, "06d.pc") == 0)
				activeOOB = &holzmarktOOBs;

            if (!activeOOB)
                return;

            for (uint32_t i = 0; i + 1 < activeOOB->size(); i += 2)
            {
                if (IsInsideVolume(coords, (*activeOOB)[i], (*activeOOB)[i + 1]))
                {
                    entered = true;
                    break;
                }
            }

            if (entered)
            {
                const auto uID = playerTouID.find(entityPtr);
                if (uID != playerTouID.end())
                    fallDamage(1337.f, uID->second);

                entered = false;
            }
        }
#ifdef DEBUG_LOGGING
        //printf("entity=0x%p, coords=(%.3f, %.3f, %.3f)\n", entityPtr, outVec3[2], outVec3[1], outVec3[0]);
#endif
    }
}

bool g_everyoneHasKnife = false;
bool g_loadoutPresetsEnabled = false;
uint32_t g_selectedLoadoutPresetIndex = 0;
static uint8_t __fastcall InventoryAssign_Detour(void* thisPtr, void* /*unknown register*/, uint32_t weaponId, int quantity, int reason, int ammo)
{
#ifdef DEBUG_LOGGING
    //printf("inventory: 0x%p, weaponId=0x%X, quantity=%d, reason=%d, ammo=%d\n", thisPtr, weaponId, quantity, reason, ammo);
#endif
    if (g_loadoutPresetsEnabled)
    {
        void* playerObject = inventoryToPlayer[thisPtr];
        uint32_t playerTeam = 0;

        if (playerObject)
            playerTeam = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(playerObject) + 0x8);

        if (g_selectedLoadoutPresetIndex == 0) // Sniper only
        {
            if (weaponId == 0x16 || weaponId == 0x17)
                weaponId = 0x0;

            if (weaponId == 0x18 || weaponId == 0x19 || weaponId == 0x1A || weaponId == 0x1B)
                weaponId = 0x0;

            if (weaponId == 0x1D || weaponId == 0x8)
                weaponId = 0x0;

            if (weaponId == 0x9 || weaponId == 0xA || weaponId == 0xF)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 1) // Machine gun only
        {
            if (weaponId == 0x16 || weaponId == 0x17)
            {
                weaponId = 0x0;

                std::vector<uint32_t> machineGuns;
                if (playerTeam == 1)
                    machineGuns = { 0x18, 0x19, 0x1A, 0x1B };
                if (playerTeam == 2)
                    machineGuns = { 0x19, 0x1A };
                if (playerTeam == 3)
                    machineGuns = { 0x18, 0x1B };

                std::uniform_int_distribution<size_t> dist(0, std::size(machineGuns) - 1);
                const uint32_t randomized = machineGuns[dist(gen)];

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

            if (weaponId == 0x1D || weaponId == 0x8)
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

            if (weaponId == 0x1D || weaponId == 0x8)
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

            if (weaponId == 0x1D || weaponId == 0x8)
                weaponId = 0x0;

            if (weaponId == 0xA)
            {
                quantity = 7;
                inventoryAssign(thisPtr, 0x9, 7, -1, 0);
                inventoryAssign(thisPtr, 0xD, 1, -1, 0);
            }

            if (weaponId == 0xF)
                weaponId = 0x0;
        }

        if (g_selectedLoadoutPresetIndex == 4) // No explosives
        {
            if (weaponId == 0x1D || weaponId == 0x8)
                weaponId = 0x0;

            if (weaponId == 0x9 || weaponId == 0xA || weaponId == 0xF)
                weaponId = 0x0;
        }
    }

    if (weaponId == 0x11 && g_everyoneHasKnife)
        inventoryAssign(thisPtr, 0xC, 1, -1, 0);

    return inventoryAssign(thisPtr, weaponId, quantity, reason, ammo);
}

static void __fastcall AutoBalanceUpdate_Detour(void* funcPtr, void* /*edx*/)
{
    if (!balanceToggle)
        return autoBalanceUpdate(funcPtr);

    return;
}

static std::vector<void*> injectedSpawnPoints;
static bool IsInjectedSpawnPoint(void* sp)
{
    const uint32_t uid = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(sp) + spawnPointOffset);
    return (uid & 0xFFF00000u) == 0x06900000u;
}

static bool __fastcall SpawnPointEligible_Detour(void* self, void* /*edx*/, void* actor)
{
    if (IsInjectedSpawnPoint(self))
    {
        float& cooldown = *reinterpret_cast<float*>(reinterpret_cast<char*>(self) + spawnCooldownOffset);
        if (cooldown < 5.0f)
            cooldown = 69.0f;
    }
    return spawnPointEligible(self, actor);
}

#ifdef DEBUG_LOGGING
static void PrintSpawnPoint(const char* tag, void* sp)
{
    const uint32_t uid = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(sp) + spawnPointOffset);
    const float x = *reinterpret_cast<float*>(reinterpret_cast<char*>(sp) + spawnPosXOffset);
    const float y = *reinterpret_cast<float*>(reinterpret_cast<char*>(sp) + spawnPosYOffset);
    const float z = *reinterpret_cast<float*>(reinterpret_cast<char*>(sp) + spawnPosZOffset);
    const uint32_t posture = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(sp) + spawnPostureOffset);
    const uint32_t teamMask = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(sp) + spawnTeamOffset);
    const uint32_t modeMask = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(sp) + spawnGameModeOffset);

    printf("%s sp=0x%p uid=0x%08X inj=%d pos=(%.3f, %.3f, %.3f) posture=0x%X team=0x%X mode=0x%X\n",
        tag,
        sp,
        uid,
        IsInjectedSpawnPoint(sp) ? 1 : 0,
        x, y, z,
        posture,
        teamMask,
        modeMask);
}
#endif

static double __cdecl SpawnPointScore_Detour(void* spawnPoint, void* actor)
{
    const double base = spawnPointScore(spawnPoint, actor);
    if (IsInjectedSpawnPoint(spawnPoint))
    {
        const double boosted = clamp(base * 1.5, 0.5, 1.0);
#ifdef DEBUG_LOGGING
        printf("[SPAWN_SCORE] injected base=%.6f boosted=%.6f\n", base, boosted);
        PrintSpawnPoint("[SPAWN_SCORE]", spawnPoint);
#endif
        return boosted;
    }

#ifdef DEBUG_LOGGING
    {
        //printf("[SPAWN_SCORE]============VANILLA============\n");
        //printf("[SPAWN_SCORE] vanilla base=%.6f\n", base);
        //PrintSpawnPoint("[SPAWN_SCORE]", spawnPoint);
        //printf("[SPAWN_SCORE]=============VANILLA END=============\n");
    }
#endif
    return base;
}

const std::vector<spawnCoords> fuelDumpSpawns = {
    { 107.83f, -6.45f, -89.65f, russiaSpawn, crouch },
    { 257.87f, 9.7f, -137.52f, germanySpawn, crouch }
};
const std::vector<spawnCoords> ubahnSpawns = {
    { -55.64f, -15.5f, -71.16f, russiaSpawn, prone },
    { 76.52f, -15.2f, -80.22f, germanySpawn, prone },
    { -37.73f, -15.5f, 54.42f, russiaSpawn, prone },
    { 40.64f, -15.71f, -39.19f, germanySpawn, prone }
};
const std::vector<spawnCoords> tempelhofSpawns = {
    { 95.72f, -7.48f, 27.88f, russiaSpawn, stand },
    { -98.89f, -19.11f, -40.56f, germanySpawn, prone }
};

static const std::vector<spawnCoords>* activeSpawns = nullptr;
static bool spawnsInjected = false;
static void* __fastcall SpawnPointInit_Detour(void* self, void* /*edx*/, int a2, int** a3)
{
    if (customSpawns)
    {
        if (strcmp(curLevel, "01b.pc") == 0)
            activeSpawns = &fuelDumpSpawns;
        if (strcmp(curLevel, "04a.pc") == 0)
            activeSpawns = &ubahnSpawns;
        if (strcmp(curLevel, "08d.pc") == 0)
            activeSpawns = &tempelhofSpawns;
    }

    static bool isInHook = false;
    if (isInHook)
        return spawnPointInit(self, a2, a3);

    isInHook = true;

    void* sp = spawnPointInit(self, a2, a3);

    if (activeSpawns && !spawnsInjected)
    {
        uint32_t origuID = *(uint32_t*)((char*)sp + spawnPointOffset);
        static uint32_t injecteduID = 0x06900000u;

        static bool s_seeded = false;

        if (!s_seeded)
        {
            s_seeded = true;

            uint32_t seed = 0x6900000u | (origuID & 0xFFFFFu);
            if (seed == 0x6900000u)
                seed = 0x6900001u;

            injecteduID = seed;
        }

        auto emitSpawn = [&](const spawnCoords entry, uint32_t modeMask, uint32_t teamMask)
            {
                void* newSpawn = operatorNew(spawnPointSize);
                if (!newSpawn)
                    return;

                memcpy(newSpawn, sp, spawnPointSize);

                uint32_t newuID = ++injecteduID;

                *(uint32_t*)((char*)newSpawn + spawnPointOffset) = newuID;
                *(float*)((char*)newSpawn + spawnPosXOffset) = entry.x;
                *(float*)((char*)newSpawn + spawnPosYOffset) = entry.y;
                *(float*)((char*)newSpawn + spawnPosZOffset) = entry.z;
                *(uint32_t*)((char*)newSpawn + spawnPostureOffset) = entry.posture;

                *(uint32_t*)((char*)newSpawn + spawnTeamOffset) = teamMask;
                *(uint32_t*)((char*)newSpawn + spawnGameModeOffset) = modeMask;
#ifdef DEBUG_LOGGING       
                printf("[SPAWN] new uid=0x%08X teamMask=0x%X pos=(%.3f, %.3f, %.3f) clone=0x%p template=0x%p\n",
                    newuID, teamMask,
                    entry.x, entry.y, entry.z,
                    newSpawn, sp);
#endif
                spawnPointInject(spawnListTable, newSpawn);
                injectedSpawnPoints.push_back(newSpawn);
            };

        for (const auto& entry : *activeSpawns)
            emitSpawn(entry, 0x18, entry.teamMask);

        spawnsInjected = true;
    }

    isInHook = false;
    return sp;
}

static void* __fastcall SpawnPointErase_Detour(void* self, void* /*edx*/, uint8_t flags)
{
    if (spawnsInjected)
    {
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

        activeSpawns = nullptr;
        spawnsInjected = false;
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
        isHost = *(const uint8_t*)0x7814F5;
        curLevel = (const char*)0x8185A0;

        if (strcmp(curLevel, "ntend.pc") == 0 && isPopulated)
        {
            isPopulated = false;
            g_playerDirectory.clear();
            uIdToPlayer.clear();
            playerTouID.clear();
            g_uIdToIp.clear();
            inventoryToPlayer.clear();
            g_activeEntities.clear();
            g_entityCoords.clear();
            wsprintfW(greetBuffer, L"Host currently not in-game");
        }

        Sleep(1);
    }
}

static void Init()
{
#ifdef DEBUG_LOGGING
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    if (MH_Initialize() != MH_OK)
        return;
#ifdef LAN
    if (MH_CreateHookApi(L"WSOCK32.dll", "bind", reinterpret_cast<void*>(&bind_Detour), reinterpret_cast<void**>(&WS2bind)) != MH_OK)
        return;
#endif
    if (MH_CreateHook(reinterpret_cast<void*>(ChatCooldownAddr),
        ChatCooldown_Detour,
        reinterpret_cast<void**>(&chatCooldown)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(DirectInputAddr),
        DirectInput_Detour,
        reinterpret_cast<void**>(&directInput)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(PlayerConstructorAddr),
        PlayerConstructor_Detour,
        reinterpret_cast<void**>(&playerConstructor)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(IPJoinSendAddr),
        IPJoinSend_Detour,
        reinterpret_cast<void**>(&ipJoinSend)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(PlayerFetchAddr),
        PlayerFetch_Detour,
        reinterpret_cast<void**>(&playerFetch)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(InventoryAssignAddr),
        InventoryAssign_Detour,
        reinterpret_cast<void**>(&inventoryAssign)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(AutoBalanceUpdateAddr),
        AutoBalanceUpdate_Detour,
        reinterpret_cast<void**>(&autoBalanceUpdate)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(PlayerGetCoordsAddr),
        PlayerGetCoords_Detour,
        reinterpret_cast<void**>(&playerGetCoords)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(SpawnPointScoreAddr),
        SpawnPointScore_Detour,
        reinterpret_cast<void**>(&spawnPointScore)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(SpawnPointInitAddr),
        SpawnPointInit_Detour,
        reinterpret_cast<void**>(&spawnPointInit)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(SpawnPointEraseAddr),
        SpawnPointErase_Detour,
        reinterpret_cast<void**>(&spawnPointErase)) != MH_OK)
        return;

    if (MH_CreateHook(reinterpret_cast<void*>(SpawnPointEligibleAddr),
        SpawnPointEligible_Detour,
        reinterpret_cast<void**>(&spawnPointEligible)) != MH_OK)
        return;

    MH_EnableHook(MH_ALL_HOOKS);
    InstallDirect3DHook();
    CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(Thread), nullptr, 0, nullptr);
    wsprintfW(greetBuffer, L"Host currently not in-game");
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hInst);
        Init();
    }
    return TRUE;
}