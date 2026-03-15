#pragma once
#include <cstdint>

static constexpr double clamp(double v, double lo, double hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

using SpawnPointEligibleFn = bool(__thiscall*)(void*, void*);
static SpawnPointEligibleFn spawnPointEligible = nullptr;
bool __fastcall SpawnPointEligible_Detour(void* self, void* /*edx*/, void* actor);

using SpawnPointScoreFn = double(__cdecl*)(void*, void*);
static SpawnPointScoreFn spawnPointScore = nullptr;
double __cdecl SpawnPointScore_Detour(void* spawnPoint, void* actor);

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
enum teamSpawnMask
{
    germanySpawn = 0x3,
    russiaSpawn = 0x5
};

constexpr size_t spawnGameModeOffset = 0x34;
constexpr size_t spawnCooldownOffset = 0x38; // float, compared against 5.0 in eligibility gate

constexpr uintptr_t OperatorNewAddr = 0x6A6322;
using OperatorNewFn = void* (__cdecl*)(size_t);
static OperatorNewFn operatorNew = reinterpret_cast<OperatorNewFn>(OperatorNewAddr);

using SpawnPointInitFn = void* (__thiscall*)(void*, int, int**);
static SpawnPointInitFn spawnPointInit = nullptr;
void* __fastcall SpawnPointInit_Detour(void* self, void* /*edx*/, int a2, int** a3);
extern bool customSpawns;

constexpr uintptr_t SpawnPointInjectAddr = 0x591C70;
using SpawnPointInjectFn = void* (__thiscall*)(void*, void*);
static SpawnPointInjectFn spawnPointInject = reinterpret_cast<SpawnPointInjectFn>(SpawnPointInjectAddr);

constexpr uintptr_t SpawnListTableAddr = 0x75A5C8;
static void* spawnListTable = reinterpret_cast<void*>(SpawnListTableAddr);

using SpawnPointEraseFn = void* (__thiscall*)(void*, uint8_t);
static SpawnPointEraseFn spawnPointErase = nullptr;
void* __fastcall SpawnPointErase_Detour(void* self, void* /*edx*/, uint8_t flags);

extern void hookSpawns();