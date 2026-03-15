#include "players.h"
#include "inventories.h"
#include "../dllmain.h"
#include "../MinHook.h"

#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

std::unordered_map<uint32_t, void*> uIdToPlayer = {};
std::unordered_map<uint32_t, std::string> uIdToIP = {};
std::unordered_map<void*, uint32_t> playerTouID = {};
void* __fastcall PlayerConstructor_Detour(void* thisPtr, void* /*unknown register*/, uint32_t uID, int a3, int a4)
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

std::unordered_set<std::string> bannedIPAddresses = {};
void __cdecl PlayerJoinIPMap_Callback(uint32_t uID, uint32_t ip)
{
    in_addr addr{};
    addr.s_addr = ip;
    char ipStr[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);
    uIdToIP[uID] = ipStr;
#ifdef DEBUG_LOGGING
    printf("PlayerJoinSend: Mapped uID %d to IP %s\n", uID, ipStr);
#endif
}

int __cdecl IPJoinSend_Detour(int connPtr, int msgPtr)
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

    const auto [_, inserted] = bannedIPAddresses.insert(ipAddress);
    return inserted;
}
bool unBanIpAddress(const std::string& ipAddress)
{
    return bannedIPAddresses.erase(ipAddress);
}
bool isIpAddressBanned(const std::string& ipAddress)
{
    return !ipAddress.empty() && bannedIPAddresses.count(ipAddress) > 0;
}

std::unordered_map<std::wstring, PlayerFetchEntry> playerToName = {};
bool isPopulated = false;
wchar_t greetBuffer[36];
std::unordered_map<void*, playerCoords> entityCoords = {};
std::unordered_set<void*> activeEntities = {};
uint32_t __cdecl PlayerFetch_Detour(Fetch* fetchStruct)
{
    isPopulated = (fetchStruct->uID > 0);
    void* playerObject = uIdToPlayer.count(fetchStruct->uID) ? uIdToPlayer[fetchStruct->uID] : nullptr;
    void* inventoryPtr = isHost ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(playerObject) + 0x1E8) : nullptr;

    if (inventoryPtr)
        inventoryToPlayer[inventoryPtr] = playerObject;

    const std::wstring nameKey = fetchStruct->nickname;
    PlayerFetchEntry& entry = playerToName[nameKey];

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
        const auto it = uIdToIP.find(fetchStruct->uID);
        if (it != uIdToIP.end())
            entry.ipAddress = it->second;
    }
#ifdef DEBUG_LOGGING
    printf("PlayerFetch called for uID=%d, isJoining=%d, resolved IP=%s\n",
        fetchStruct->uID, fetchStruct->isJoining, entry.ipAddress.c_str());
#endif
    if (fetchStruct->isJoining && isIpAddressBanned(entry.ipAddress))
        kick(fetchStruct->uID, 4);

    if (fetchStruct->isJoining)
        activeEntities.insert(playerObject);
    else
    {
        activeEntities.erase(playerObject);
        uIdToPlayer.erase(fetchStruct->uID);
        uIdToIP.erase(fetchStruct->uID);
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
    { -105.f, -2.9f, -142.5f }, { -102.f, -1.2f, -140.4f } // Lifting barrier
};
const std::vector<playerCoords> ubahnOOBs = {
    { 30.41f, -5.4f, 5.28f }, { 35.75f, -4.12f, 9.8f }, // Neighbor room with invisible indoor walls
    { -46.2f, -1.34f, 48.f }, { -17.5f, 0.f, 80.f }, // Right side from the roof spawn near railings
    { 9.4f, -2.51f, -130.f }, { 30.9f, 0.f, -127.f } // Barrier connected with stone fence
};
const std::vector<playerCoords> holzmarktOOBs = {
    { -78.9f, -3.f, 7.8f }, { -75.5f, -0.2f, 4.9f } // Lifting barrier near sewers
};

bool antiOOB = true;
void __fastcall PlayerGetCoords_Detour(void* thisPtr, void* /*edx*/, float* outVec3)
{
    void* entityPtr = nullptr;
    __asm { mov entityPtr, esi }

    playerGetCoords(thisPtr, outVec3);

    if (activeEntities.contains(entityPtr))
    {
        const playerCoords coords{ outVec3[2], outVec3[1], outVec3[0] };
        entityCoords[entityPtr] = coords;

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

void hookPlayers()
{
    MH_CreateHook(reinterpret_cast<void*>(PlayerConstructorAddr),
        PlayerConstructor_Detour, reinterpret_cast<void**>(&playerConstructor));

    MH_CreateHook(reinterpret_cast<void*>(IPJoinSendAddr),
        IPJoinSend_Detour, reinterpret_cast<void**>(&ipJoinSend));

    MH_CreateHook(reinterpret_cast<void*>(PlayerFetchAddr),
        PlayerFetch_Detour, reinterpret_cast<void**>(&playerFetch));

    MH_CreateHook(reinterpret_cast<void*>(PlayerGetCoordsAddr),
        PlayerGetCoords_Detour, reinterpret_cast<void**>(&playerGetCoords));
}