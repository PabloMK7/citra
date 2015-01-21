// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/symbols.h"

TSymbolsMap g_symbols;

namespace Symbols
{
    bool HasSymbol(u32 _address)
    {
        return g_symbols.find(_address) != g_symbols.end();
    }

    void Add(u32 _address, const std::string& _name, u32 _size, u32 _type)
    {
        if (!HasSymbol(_address))
        {
            TSymbol symbol;
            symbol.address = _address;
            symbol.name = _name;
            symbol.size = _size;
            symbol.type = _type;

            g_symbols.insert(TSymbolsPair(_address, symbol));
        }
    }

    TSymbol GetSymbol(u32 _address)
    {
        TSymbolsMap::iterator foundSymbolItr;
        TSymbol symbol;

        foundSymbolItr = g_symbols.find(_address);
        if (foundSymbolItr != g_symbols.end())
        {
            symbol = (*foundSymbolItr).second;
        }

        return symbol;
    }
    const std::string GetName(u32 _address)
    {
        return GetSymbol(_address).name;
    }

    void Remove(u32 _address)
    {
        g_symbols.erase(_address);
    }

    void Clear()
    {
        g_symbols.clear();
    }
}
