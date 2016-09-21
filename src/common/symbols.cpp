// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/symbols.h"

TSymbolsMap g_symbols;

namespace Symbols {
bool HasSymbol(u32 address) {
    return g_symbols.find(address) != g_symbols.end();
}

void Add(u32 address, const std::string& name, u32 size, u32 type) {
    if (!HasSymbol(address)) {
        TSymbol symbol;
        symbol.address = address;
        symbol.name = name;
        symbol.size = size;
        symbol.type = type;

        g_symbols.emplace(address, symbol);
    }
}

TSymbol GetSymbol(u32 address) {
    const auto iter = g_symbols.find(address);

    if (iter != g_symbols.end())
        return iter->second;

    return {};
}

const std::string GetName(u32 address) {
    return GetSymbol(address).name;
}

void Remove(u32 address) {
    g_symbols.erase(address);
}

void Clear() {
    g_symbols.clear();
}
}
