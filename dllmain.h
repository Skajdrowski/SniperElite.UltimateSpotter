#pragma once
#include <iostream>
#include <vector>

#include <cstdint>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <string>

constexpr uintptr_t ChatCooldownAddr = 0x4832D0;
constexpr uintptr_t OperatorNewAddr = 0x6A6322;
constexpr uintptr_t DirectInputAddr = 0x40AC10;
constexpr uintptr_t PlayerFetchAddr = 0x6080C0;
constexpr uintptr_t IPJoinSendAddr = 0x446790;
constexpr uintptr_t PlayerConstructorAddr = 0x56F390;
constexpr uintptr_t PlayerGetCoordsAddr = 0x533880;
constexpr uintptr_t FallDamageAddr = 0x525770;
constexpr uintptr_t InventoryAssignAddr = 0x519C90;
constexpr uintptr_t InventoryCapAddr = 0x511420;
constexpr uintptr_t KickPlayerAddr = 0x454610;
constexpr uintptr_t AutoBalanceUpdateAddr = 0x617E20;
constexpr uintptr_t LoadingFlagAddr = 0x7A35A9;
constexpr uintptr_t SpawnPointScoreAddr = 0x61D500;
constexpr uintptr_t SpawnListTableAddr = 0x75A5C8;
constexpr uintptr_t SpawnPointInitAddr = 0x591A40;
constexpr uintptr_t SpawnPointInjectAddr = 0x591C70;
constexpr uintptr_t SpawnPointEraseAddr = 0x591D20;
constexpr uintptr_t SpawnPointEligibleAddr = 0x591700;

extern bool isHost;
extern bool customSpawnsToggle;
extern bool antiOOB;
extern const char* curLevel;

static constexpr double clamp(double v, double lo, double hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

using ChatCooldownFn = uint8_t(__cdecl*)(uint32_t key);
static ChatCooldownFn chatCooldown = nullptr;

struct Fetch {
    uint8_t unknownShit[8];
    wchar_t nickname[16];
    uint8_t unknownShit_[20];
    uint32_t uID;
    uint8_t isJoining;
};

using DirectInputFn = uint32_t(__cdecl*)();
static DirectInputFn directInput = nullptr;

using PlayerConstructorFn = void* (__thiscall*)(void*, uint32_t, int, int);
static PlayerConstructorFn playerConstructor = nullptr;
extern bool isPopulated;
extern wchar_t greetBuffer[36];
extern std::map<uint32_t, void*> uIdToPlayer;

using KickFn = uint8_t(__cdecl*)(int uID, int reason);
static KickFn kick = reinterpret_cast<KickFn>(KickPlayerAddr);

using IPJoinSendFn = int(__cdecl*)(int, int);
static IPJoinSendFn ipJoinSend = nullptr;
extern bool banIpAddress(const std::string& ipAddress);
extern bool unBanIpAddress(const std::string& ipAddress);
extern bool isIpAddressBanned(const std::string& ipAddress);

using PlayerFetchFn = uint32_t(__cdecl*)(Fetch*);
static PlayerFetchFn playerFetch = nullptr;
struct PlayerFetchEntry
{
    uint32_t uid = 0;
    void* player = nullptr;
    void* inventory = nullptr;
    bool isOnline = false;
    std::wstring displayName;
    std::string ipAddress;
};
extern std::map<std::wstring, PlayerFetchEntry> g_playerDirectory;

struct playerCoords
{
    float x;
    float y;
    float z;
};
using PlayerGetCoordsFn = void(__thiscall*)(void*, float*);
static PlayerGetCoordsFn playerGetCoords = nullptr;
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

using FallDamageFn = uint32_t(__cdecl*)(float, uint32_t);
static FallDamageFn fallDamage = reinterpret_cast <FallDamageFn>(FallDamageAddr);

using InventoryAssignFn = uint8_t(__thiscall*)(void*, uint32_t, int, int, int);
static InventoryAssignFn inventoryAssign = nullptr;
extern bool g_everyoneHasKnife;
extern bool g_loadoutPresetsEnabled;
extern uint32_t g_selectedLoadoutPresetIndex;

using InventoryCapFn = uint32_t(__cdecl*)(uint32_t);
static InventoryCapFn inventoryCap = nullptr;

using AutoBalanceUpdateFn = void(__thiscall*)(void*);
static AutoBalanceUpdateFn autoBalanceUpdate = nullptr;
static bool balanceToggle = false;

using SpawnPointScoreFn = double(__cdecl*)(void*, void*);
static SpawnPointScoreFn spawnPointScore = nullptr;

using SpawnPointEligibleFn = bool(__thiscall*)(void*, void*);
static SpawnPointEligibleFn spawnPointEligible = nullptr;

struct spawnCoords
{
    float x;
    float y;
    float z;
    uint32_t teamMask;
    uint32_t posture;
};

constexpr size_t spawnPointSize = 0x3C;
constexpr size_t spawnPointOffset = 0x4;

constexpr size_t spawnPosXOffset = 0x18;
constexpr size_t spawnPosYOffset = 0x14;
constexpr size_t spawnPosZOffset = 0x10;

constexpr size_t spawnPostureOffset = 0x2C;
enum posture
{
    stand = 0x0,
    crouch = 0x1,
    prone = 0x2
};

constexpr size_t spawnTeamOffset = 0x30;
enum team
{
    germany = 0x3,
    russia = 0x5
};

constexpr size_t spawnGameModeOffset = 0x34;
constexpr size_t spawnCooldownOffset = 0x38; // float, compared against 5.0 in eligibility gate

using OperatorNewFn = void* (__cdecl*)(size_t);
static OperatorNewFn operatorNew = reinterpret_cast<OperatorNewFn>(OperatorNewAddr);

using SpawnPointInitFn = void* (__thiscall*)(void*, int, int**);
static SpawnPointInitFn spawnPointInit = nullptr;

using SpawnPointInjectFn = void* (__thiscall*)(void*, void*);
static SpawnPointInjectFn spawnPointInject = reinterpret_cast<SpawnPointInjectFn>(SpawnPointInjectAddr);

static void* spawnListTable = reinterpret_cast<void*>(SpawnListTableAddr);

using SpawnPointEraseFn = void* (__thiscall*)(void*, uint8_t);
static SpawnPointEraseFn spawnPointErase = nullptr;