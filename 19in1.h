#pragma once
#include <vadefs.h>

extern bool unlockMaps;
extern void listMaps();

constexpr uintptr_t MpMapTableBaseAddr = 0x757B28;
constexpr size_t MpMapRecordSize = 0x28;
constexpr size_t MpMapRecordCount = 34;
constexpr size_t MpMapRecordFlagsOffset = 0x20;