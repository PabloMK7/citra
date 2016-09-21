// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <utility>
#include "common/common_types.h"

struct TSymbol {
    u32 address = 0;
    std::string name;
    u32 size = 0;
    u32 type = 0;
};

typedef std::map<u32, TSymbol> TSymbolsMap;
typedef std::pair<u32, TSymbol> TSymbolsPair;

namespace Symbols {
bool HasSymbol(u32 address);

void Add(u32 address, const std::string& name, u32 size, u32 type);
TSymbol GetSymbol(u32 address);
const std::string GetName(u32 address);
void Remove(u32 address);
void Clear();
}
