#pragma once
#include "dxsdk/include/d3d8.h"

#include "iathook.h"
#include "gui/GUI.h"

#pragma comment(lib, "dxsdk/lib/d3d8.lib")
#pragma comment(lib, "dxsdk/lib/d3dx8.lib")

using Direct3DCreate8Fn = decltype(&Direct3DCreate8);
static Direct3DCreate8Fn g_direct3DCreate8Original = nullptr;

using CreateDeviceFn = HRESULT(__stdcall*)(IDirect3D8*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice8**);
static CreateDeviceFn g_createDeviceOriginal = nullptr;

using EndSceneFn = HRESULT(__stdcall*)(IDirect3DDevice8*);
static EndSceneFn g_endSceneOriginal = nullptr;

using ReleaseFn = ULONG(__stdcall*)(IDirect3DDevice8*);
static ReleaseFn g_releaseOriginal = nullptr;

using ResetFn = HRESULT(__stdcall*)(IDirect3DDevice8*, D3DPRESENT_PARAMETERS*);
static ResetFn g_resetOriginal = nullptr;

static HRESULT __stdcall EndScene_Detour(IDirect3DDevice8* device)
{
    if (GetAsyncKeyState(VK_INSERT) & 1)
        GUI::Toggle();
    if (GetAsyncKeyState(VK_HOME) & 1)
        GUI::ToogleGreeting();

    if (!GUI::isInitialized)
        GUI::Start(device);

    if (GUI::isVisible)
        GUI::Render();

    if (GUI::isGreeting)
        GUI::RenderGreeting();

    return g_endSceneOriginal(device);
}

static ULONG __stdcall Release_Detour(IDirect3DDevice8* device)
{
    const ULONG refCount = g_releaseOriginal(device);
    if (refCount == 0)
    {
        GUI::Shutdown();
        g_endSceneOriginal = nullptr;
        g_releaseOriginal = nullptr;
        g_resetOriginal = nullptr;
    }
    return refCount;
}

static HRESULT __stdcall Reset_Detour(IDirect3DDevice8* device, D3DPRESENT_PARAMETERS* params)
{
    GUI::OnDeviceLost();
    const HRESULT result = g_resetOriginal(device, params);
    if (SUCCEEDED(result))
        GUI::OnDeviceReset(device);
    return result;
}

static void HookDevice(IDirect3DDevice8* device)
{
    if (!device)
        return;

    auto vtable = *reinterpret_cast<void***>(device);
    if (!vtable)
        return;

    DWORD oldProtect;

    if (!g_endSceneOriginal)
    {
        constexpr size_t EndSceneIndex = 35;
        VirtualProtect(&vtable[EndSceneIndex], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        g_endSceneOriginal = reinterpret_cast<EndSceneFn>(vtable[EndSceneIndex]);
        vtable[EndSceneIndex] = reinterpret_cast<void*>(&EndScene_Detour);
        VirtualProtect(&vtable[EndSceneIndex], sizeof(void*), oldProtect, &oldProtect);
    }

    if (!g_releaseOriginal)
    {
        constexpr size_t ReleaseIndex = 2;
        VirtualProtect(&vtable[ReleaseIndex], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        g_releaseOriginal = reinterpret_cast<ReleaseFn>(vtable[ReleaseIndex]);
        vtable[ReleaseIndex] = reinterpret_cast<void*>(&Release_Detour);
        VirtualProtect(&vtable[ReleaseIndex], sizeof(void*), oldProtect, &oldProtect);
    }

    if (!g_resetOriginal)
    {
        constexpr size_t ResetIndex = 14;
        VirtualProtect(&vtable[ResetIndex], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        g_resetOriginal = reinterpret_cast<ResetFn>(vtable[ResetIndex]);
        vtable[ResetIndex] = reinterpret_cast<void*>(&Reset_Detour);
        VirtualProtect(&vtable[ResetIndex], sizeof(void*), oldProtect, &oldProtect);
    }
}

static HRESULT __stdcall CreateDevice_Detour(IDirect3D8* self, UINT adapter, D3DDEVTYPE deviceType, HWND hFocusWindow,
    DWORD behaviorFlags, D3DPRESENT_PARAMETERS* presentationParameters, IDirect3DDevice8** returnedDeviceInterface)
{
    const HRESULT result = g_createDeviceOriginal(self, adapter, deviceType, hFocusWindow, behaviorFlags, presentationParameters, returnedDeviceInterface);
    if (SUCCEEDED(result) && returnedDeviceInterface && *returnedDeviceInterface)
        HookDevice(*returnedDeviceInterface);
    return result;
}

static void HookCreateDevice(IDirect3D8* object)
{
    if (!object || g_createDeviceOriginal)
        return;

    auto vtable = *reinterpret_cast<void***>(object);
    if (!vtable)
        return;

    constexpr size_t CreateDeviceIndex = 15;
    DWORD oldProtect;
    VirtualProtect(&vtable[CreateDeviceIndex], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    g_createDeviceOriginal = reinterpret_cast<CreateDeviceFn>(vtable[CreateDeviceIndex]);
    vtable[CreateDeviceIndex] = reinterpret_cast<void*>(&CreateDevice_Detour);
    VirtualProtect(&vtable[CreateDeviceIndex], sizeof(void*), oldProtect, &oldProtect);
}

static IDirect3D8* WINAPI Direct3DCreate8_Detour(UINT sdkVersion)
{
    if (!g_direct3DCreate8Original)
        return nullptr;

    IDirect3D8* instance = g_direct3DCreate8Original(sdkVersion);
    if (instance)
        HookCreateDevice(instance);
    return instance;
}

static void InstallDirect3DHook()
{
    static bool attempted = false;
    if (attempted || g_direct3DCreate8Original)
        return;

    attempted = true;

    auto originals = IATHook::Replace(GetModuleHandle(nullptr), "d3d8.dll",
        std::make_tuple("Direct3DCreate8", reinterpret_cast<void*>(Direct3DCreate8_Detour)));

    auto it = originals.find("Direct3DCreate8");
    if (it != originals.end())
        g_direct3DCreate8Original = reinterpret_cast<Direct3DCreate8Fn>(it->second.get());
}
