// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <utility>

#include "common/common_types.h"

struct TSymbol
{
    TSymbol() :
        address(0),
        size(0),
        type(0)
    {}
    u32     address;
    std::string name;
    u32     size;
    u32     type;
};

typedef std::map<u32, TSymbol> TSymbolsMap;
typedef std::pair<u32, TSymbol> TSymbolsPair;

namespace Symbols
{
    bool HasSymbol(u32 _address);

    void Add(u32 _address, const std::string& _name, u32 _size, u32 _type);
    TSymbol GetSymbol(u32 _address);
    const std::string GetName(u32 _address);
    void Remove(u32 _address);
    void Clear();
}

