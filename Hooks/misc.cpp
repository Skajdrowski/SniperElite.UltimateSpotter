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

constexpr size_t configSize = 0x5F5;
static std::vector<uint8_t> g_lastSaved = {};
static std::wstring bytes_to_hex(const uint8_t* data, size_t size)
{
    static constexpr wchar_t kHex[] = L"0123456789ABCDEF";
    std::wstring out;
    out.resize(size * 2);
    for (size_t i = 0; i < size; ++i)
    {
        const uint8_t b = data[i];
        out[i * 2 + 0] = kHex[b >> 4];
        out[i * 2 + 1] = kHex[b & 0xF];
    }
    return out;
}

static int hex_value(wchar_t c)
{
    if (c >= L'0' && c <= L'9')
        return c - L'0';
    if (c >= L'a' && c <= L'f')
        return 10 + (c - L'a');
    if (c >= L'A' && c <= L'F')
        return 10 + (c - L'A');
    return -1;
}

static bool hex_to_bytes(const wchar_t* hex, std::vector<uint8_t>& out, size_t expectedSize)
{
    if (!hex)
        return false;

    const size_t len = wcslen(hex);
    if (len != expectedSize * 2)
        return false;

    out.resize(expectedSize);
    for (size_t i = 0; i < expectedSize; ++i)
    {
        const int hi = hex_value(hex[i * 2 + 0]);
        const int lo = hex_value(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

static const std::wstring ConfigKey = L"gameConfig";

static void readLobbyConfig(uint8_t* cfg)
{
    if (!cfg || !g_lastSaved.empty())
        return;

    std::vector<wchar_t> buf;
    buf.resize(configSize * 2 + 4);
    const DWORD read = GetPrivateProfileStringW(L"LOBBY", ConfigKey.c_str(), L"", buf.data(), static_cast<DWORD>(buf.size()), iniPath);
    if (read != 0)
    {
        std::vector<uint8_t> parsed;
        if (hex_to_bytes(buf.data(), parsed, configSize))
        {
            memcpy(cfg, parsed.data(), configSize);
            g_lastSaved = parsed;
#ifdef DEBUG_LOGGING
            printf("config loaded\n");
#endif
        }
    }
}

static void saveLobbyConfig(uint8_t* cfg)
{
    if (!cfg)
        return;

    const uint8_t* cur = cfg;

    if (g_lastSaved.size() != configSize || memcmp(g_lastSaved.data(), cur, configSize) != 0)
    {
        const std::wstring hex = bytes_to_hex(cur, configSize);
        WritePrivateProfileStringW(L"LOBBY", ConfigKey.c_str(), hex.c_str(), iniPath);
        g_lastSaved.assign(cur, cur + configSize);
#ifdef DEBUG_LOGGING
        printf("config saved\n");
#endif
    }
}

static int __fastcall LobbyTemplateCopy_Detour(void* thisPtr, void* edx, void* src)
{
    const int og = lobbyTemplateCopy(thisPtr, src);

    if (thisPtr == reinterpret_cast<void*>(0x7C3958))
    {
        uint8_t* data = reinterpret_cast<uint8_t*>(thisPtr);

        if (edx == reinterpret_cast<void*>(0x706738))
            readLobbyConfig(data);

        if (edx == reinterpret_cast<void*>(0x706C60))
            saveLobbyConfig(data);
    }

    return og;
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

    MH_CreateHook(reinterpret_cast<void*>(LobbyTemplateCopyAddr),
        LobbyTemplateCopy_Detour, reinterpret_cast<void**>(&lobbyTemplateCopy));
}