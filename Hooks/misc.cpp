#include "misc.h"
#include "../MinHook.h"
#include "../gui/GUI.h"

uint32_t __cdecl DirectInput_Detour()
{
    if (GUI::isVisible)
        return 0;

    return directInput();
}

//#define LAN
#ifdef LAN
int WSAAPI bind_Detour(SOCKET s, const sockaddr* name, int namelen)
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

uint8_t __cdecl ChatCooldown_Detour(uint32_t key)
{
    const uint8_t result = chatCooldown(key);

    float* gCooldown = (float*)0x78A8EC;

    if (key == 13 && gCooldown)
        *gCooldown = 1.0f;

    return result;
}

bool autoBalance = false;
void __fastcall AutoBalanceUpdate_Detour(void* funcPtr, void* /*edx*/)
{
    if (!autoBalance)
        return autoBalanceUpdate(funcPtr);

    return;
}

void hookMisc()
{
#ifdef LAN
    MH_CreateHookApi(L"WSOCK32.dll", "bind", reinterpret_cast<void*>(&bind_Detour), reinterpret_cast<void**>(&WS2bind));
#endif
    MH_CreateHook(reinterpret_cast<void*>(DirectInputAddr),
        DirectInput_Detour, reinterpret_cast<void**>(&directInput));

    MH_CreateHook(reinterpret_cast<void*>(ChatCooldownAddr),
        ChatCooldown_Detour, reinterpret_cast<void**>(&chatCooldown));

    MH_CreateHook(reinterpret_cast<void*>(AutoBalanceUpdateAddr),
        AutoBalanceUpdate_Detour, reinterpret_cast<void**>(&autoBalanceUpdate));
}