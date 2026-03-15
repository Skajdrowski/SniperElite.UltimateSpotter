#include "dllmain.h"
#include "MinHook.h"
#include "dxHook.h"

bool isHost = false;
const char* curLevel = nullptr;

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
            playerToName.clear();
            uIdToPlayer.clear();
            playerTouID.clear();
            uIdToIP.clear();
            inventoryToPlayer.clear();
            activeEntities.clear();
            entityCoords.clear();
            wsprintfW(greetBuffer, L"Host currently not in-game");
        }

        Sleep(1);
    }
}

wchar_t iniPath[MAX_PATH];
static void config_init()
{
    HMODULE hMod = NULL;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<const wchar_t*>(&config_init),
        &hMod
    );
    GetModuleFileNameW(hMod, iniPath, MAX_PATH);

    wchar_t* dot = wcsrchr(iniPath, L'.');
    if (dot)
        wcscpy(dot, L".ini");

    autoBalance = GetPrivateProfileIntW(L"LOBBY", L"AutoBalance", false, iniPath);
    antiOOB = GetPrivateProfileIntW(L"LOBBY", L"AntiOOB", true, iniPath);
    customSpawns = GetPrivateProfileIntW(L"LOBBY", L"CustomSpawns", false, iniPath);

    g_selectedLoadoutPresetIndex = GetPrivateProfileIntW(L"INVENTORIES", L"LoadoutPreset", 0, iniPath);
    g_everyoneHasKnife = GetPrivateProfileIntW(L"INVENTORIES", L"EveryOneKnife", false, iniPath);
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

    hookMisc();
    hookPlayers();
    hookInvs();
    hookSpawns();

    MH_EnableHook(MH_ALL_HOOKS);
    config_init();
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