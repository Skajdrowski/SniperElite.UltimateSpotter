#pragma once
#include <iostream>
#include <vector>

#include <cstdint>
#include <map>
#include <string>

constexpr uintptr_t DisconnectIdleAddr = 0x60AD46;
constexpr uintptr_t SlotsBroadcastAddr = 0x60D360;

constexpr uintptr_t ClientHistoriesCapAddr = 0x6149CA;
constexpr uintptr_t ClientHistoryInitAddr = 0x60BE00;
constexpr uintptr_t ClientHistoryCountConnectedAddr = 0x60C0C0;
constexpr uintptr_t ClientHistoryFindByNumberAddr = 0x60C120;
constexpr uintptr_t ClientHistoryIterNextConnectedAddr = 0x60C290;

constexpr uintptr_t ClientHistoryFindFreeSlotAddr = 0x60C230;
constexpr uintptr_t ClientHistoryAddOrUpdateAddr = 0x60C9F0;

constexpr uintptr_t ClientHistoryClearByNumberAddr = 0x60C930;

constexpr uintptr_t ClientHistoryStaticBaseAddr = 0x804B78;

constexpr uintptr_t DirectInputAddr = 0x40AC10;

constexpr uintptr_t PlayerFetchAddr = 0x6080C0;
constexpr uintptr_t PlayerIPListenerAddr = 0x641D60;
constexpr uintptr_t PlayerConstructorAddr = 0x56F390;

constexpr uintptr_t InventoryAssignAddr = 0x519C90;
constexpr uintptr_t InventoryCapAddr = 0x511420;

constexpr uintptr_t KickPlayerAddr = 0x454610;
constexpr uintptr_t AutoBalanceUpdateAddr = 0x617E20;
constexpr uintptr_t loadingFlagAddr = 0x7A35A9;

extern bool isHost;

struct ClientHistoryEntry
{
    uint8_t pad_0[0x3C];
    uint32_t playerId;
    uint8_t connected;
    uint8_t pad_41[0xB];
    uint32_t value100; //likely health
    uint8_t pad_50[0x28];
    uint32_t state3;
};
static ClientHistoryEntry* clients;

constexpr uint32_t ClientHistoryMaxClients = 32;

using ClientHistoryInitFn = uint32_t(__thiscall*)(void*);
static ClientHistoryInitFn clientHistoryInit = nullptr;

using ClientHistoryCountConnectedFn = uint32_t(__thiscall*)(void*);
static ClientHistoryCountConnectedFn clientHistoryCountConnected = nullptr;

using ClientHistoryFindByuIDFn = uint8_t*(__thiscall*)(void*, uint32_t, char);
static ClientHistoryFindByuIDFn clientHistoryFindByNumber = nullptr;

using ClientHistoryIterNextConnectedFn = uint8_t*(__thiscall*)(void*, int);
static ClientHistoryIterNextConnectedFn clientHistoryIterNextConnected = nullptr;

using ClientHistoryFindFreeSlotFn = uint8_t*(__thiscall*)(void*);
static ClientHistoryFindFreeSlotFn clientHistoryFindFreeSlot = nullptr;

using ClientHistoryAddOrUpdateFn = uint8_t(__thiscall*)(void*, void*);
static ClientHistoryAddOrUpdateFn clientHistoryAddOrUpdate = nullptr;

using ClientHistoryClearByuIDFn = uint32_t(__thiscall*)(void*, uint32_t);
static ClientHistoryClearByuIDFn clientHistoryClearByuID = nullptr;

struct Fetch {
    uint8_t unknownShit[8];
    wchar_t nickname[16];
    uint8_t unknownShit_[20];
    uint32_t uID;
    uint8_t isJoining;
};

using DirectInputFn = uint32_t(__cdecl*)();
static DirectInputFn directInput = nullptr;

using SlotsBroadcastFn = void(__cdecl*)(void*);
static SlotsBroadcastFn slotsBroadcast = nullptr;

using PlayerConstructorFn = void* (__thiscall*)(void*, int, int, int);
static PlayerConstructorFn playerConstructor = nullptr;
extern bool isPopulated;
extern wchar_t greetBuffer[36];

using KickFn = uint8_t(__cdecl*)(int uID, int reason);
static KickFn kick = reinterpret_cast<KickFn>(KickPlayerAddr);

using PlayerIPListenerFn = uint32_t(__cdecl*)(void*, void**, uint32_t, uint16_t);
static PlayerIPListenerFn playerIPListener = nullptr;
extern bool banIpAddress(const std::string& ipAddress);
extern bool unBanIpAddress(const std::string& ipAddress);
extern bool isIpAddressBanned(const std::string& ipAddress);

using PlayerFetchFn = uint32_t(__cdecl*)(Fetch*);
static PlayerFetchFn playerFetch = nullptr;
struct PlayerFetchEntry
{
    uint32_t uid = 0;
    void* inventory = nullptr;
    bool isOnline = false;
    std::wstring displayName;
    std::string ipAddress;
};
extern std::map<uint32_t, void*> g_uIdToInventoryMap;
extern std::map<std::wstring, PlayerFetchEntry> g_playerDirectory;

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
