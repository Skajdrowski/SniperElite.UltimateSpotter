#include "spawns.h"
#include "../dllmain.h"
#include "../MinHook.h"
#include <vector>

static std::vector<void*> injectedSpawnPoints = {};
static bool IsInjectedSpawnPoint(void* sp)
{
    const uint32_t uid = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(sp) + spawnPointOffset);
    return (uid & 0xFFF00000u) == 0x06900000u;
}

bool __fastcall SpawnPointEligible_Detour(void* self, void* /*edx*/, void* actor)
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

double __cdecl SpawnPointScore_Detour(void* spawnPoint, void* actor)
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
    //printf("[SPAWN_SCORE]============VANILLA============\n");
    //printf("[SPAWN_SCORE] vanilla base=%.6f\n", base);
    //PrintSpawnPoint("[SPAWN_SCORE]", spawnPoint);
    //printf("[SPAWN_SCORE]=============VANILLA END=============\n");
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

bool customSpawns = false;
static const std::vector<spawnCoords>* activeSpawns = nullptr;
static bool spawnsInjected = false;
void* __fastcall SpawnPointInit_Detour(void* self, void* /*edx*/, int a2, int** a3)
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

void* __fastcall SpawnPointErase_Detour(void* self, void* /*edx*/, uint8_t flags)
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

void hookSpawns()
{
    MH_CreateHook(reinterpret_cast<void*>(SpawnPointScoreAddr),
        SpawnPointScore_Detour, reinterpret_cast<void**>(&spawnPointScore));

    MH_CreateHook(reinterpret_cast<void*>(SpawnPointEligibleAddr),
        SpawnPointEligible_Detour, reinterpret_cast<void**>(&spawnPointEligible));

    MH_CreateHook(reinterpret_cast<void*>(SpawnPointInitAddr),
        SpawnPointInit_Detour, reinterpret_cast<void**>(&spawnPointInit));

    MH_CreateHook(reinterpret_cast<void*>(SpawnPointEraseAddr),
        SpawnPointErase_Detour, reinterpret_cast<void**>(&spawnPointErase));
}