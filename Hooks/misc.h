#pragma once
#include <stdint.h>

using ChatCooldownFn = uint8_t(__cdecl*)(uint32_t key);
static ChatCooldownFn chatCooldown = nullptr;
uint8_t __cdecl ChatCooldown_Detour(uint32_t key);

using DirectInputFn = uint32_t(__cdecl*)();
static DirectInputFn directInput = nullptr;
uint32_t __cdecl DirectInput_Detour();

//#define LAN
#ifdef LAN
#include <winsock2.h>
using BindFn = int (WSAAPI*)(SOCKET, const sockaddr*, int);
static BindFn WS2bind = nullptr;
static int WSAAPI bind_Detour(SOCKET s, const sockaddr* name, int namelen);
#endif

using AutoBalanceUpdateFn = void(__thiscall*)(void*);
static AutoBalanceUpdateFn autoBalanceUpdate = nullptr;
void __fastcall AutoBalanceUpdate_Detour(void* funcPtr, void* /*edx*/);
extern bool autoBalance;

extern void hookMisc();

using LobbyTemplateCopyFn = int(__thiscall*)(void* thisPtr, void* src);
static LobbyTemplateCopyFn lobbyTemplateCopy = nullptr;