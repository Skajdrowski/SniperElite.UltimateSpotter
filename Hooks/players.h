#pragma once
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <xstring>

extern bool antiOOB;

struct Fetch {
    uint8_t unknownShit[8];
    wchar_t nickname[16];
    uint8_t unknownShit_[20];
    uint32_t uID;
    uint8_t isJoining;
};

using PlayerConstructorFn = void* (__thiscall*)(void*, uint32_t, int, int);
static PlayerConstructorFn playerConstructor = nullptr;
void* __fastcall PlayerConstructor_Detour(void* thisPtr, void* /*unknown register*/, uint32_t uID, int a3, int a4);
extern bool isPopulated;
extern wchar_t greetBuffer[36];
extern std::unordered_map<uint32_t, void*> uIdToPlayer;
extern std::unordered_map<uint32_t, std::string> uIdToIP;
extern std::unordered_map<void*, uint32_t> playerTouID;

constexpr uintptr_t KickPlayerAddr = 0x454610;
using KickFn = uint8_t(__cdecl*)(int uID, int reason);
static KickFn kick = reinterpret_cast<KickFn>(KickPlayerAddr);

using IPJoinSendFn = int(__cdecl*)(int, int);
static IPJoinSendFn ipJoinSend = nullptr;
int __cdecl IPJoinSend_Detour(int connPtr, int msgPtr);
static void __cdecl PlayerJoinIPMap_Callback(uint32_t uID, uint32_t ip);
extern bool banIpAddress(const std::string& ipAddress);
extern bool unBanIpAddress(const std::string& ipAddress);
extern bool isIpAddressBanned(const std::string& ipAddress);
extern std::unordered_set<std::string> bannedIPAddresses;

using PlayerFetchFn = uint32_t(__cdecl*)(Fetch*);
static PlayerFetchFn playerFetch = nullptr;
uint32_t __cdecl PlayerFetch_Detour(Fetch* fetchStruct);
struct PlayerFetchEntry
{
    uint32_t uid = 0;
    void* player = nullptr;
    void* inventory = nullptr;
    bool isOnline = false;
    std::wstring displayName;
    std::string ipAddress;
};
extern std::unordered_map<std::wstring, PlayerFetchEntry> playerToName;

constexpr uintptr_t GetPingAddr = 0x4529F0;
using GetPingFn = int(__cdecl*)(uint32_t uID);
static GetPingFn getPing = reinterpret_cast<GetPingFn>(GetPingAddr);

struct playerCoords
{
    float x;
    float y;
    float z;
};
using PlayerGetCoordsFn = void(__thiscall*)(void*, float*);
static PlayerGetCoordsFn playerGetCoords = nullptr;
void __fastcall PlayerGetCoords_Detour(void* thisPtr, void* /*edx*/, float* outVec3);
extern bool antiOOB;
extern std::unordered_map<void*, playerCoords> entityCoords;
extern std::unordered_set<void*> activeEntities;
static bool IsInsideVolume(const playerCoords& pos, const playerCoords& minV, const playerCoords& maxV)
{
    const float minX = (std::min)(minV.x, maxV.x);
    const float maxX = (std::max)(minV.x, maxV.x);
    const float minY = (std::min)(minV.y, maxV.y);
    const float maxY = (std::max)(minV.y, maxV.y);
    const float minZ = (std::min)(minV.z, maxV.z);
    const float maxZ = (std::max)(minV.z, maxV.z);

    return pos.x >= minX && pos.x <= maxX
        && pos.y >= minY && pos.y <= maxY
        && pos.z >= minZ && pos.z <= maxZ;
}
constexpr uintptr_t FallDamageAddr = 0x525770;
using FallDamageFn = uint32_t(__cdecl*)(float, uint32_t);
static FallDamageFn fallDamage = reinterpret_cast <FallDamageFn>(FallDamageAddr);

extern void hookPlayers();