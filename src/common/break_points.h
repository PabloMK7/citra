// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "common/common_types.h"

class DebugInterface;

struct TBreakPoint {
    u32 iAddress;
    bool bOn;
    bool bTemporary;
};

// Code breakpoints.
class BreakPoints {
public:
    typedef std::vector<TBreakPoint> TBreakPoints;
    typedef std::vector<std::string> TBreakPointsStr;

    const TBreakPoints& GetBreakPoints() {
        return m_BreakPoints;
    }

    TBreakPointsStr GetStrings() const;
    void AddFromStrings(const TBreakPointsStr& bps);

    // is address breakpoint
    bool IsAddressBreakPoint(u32 iAddress) const;
    bool IsTempBreakPoint(u32 iAddress) const;

    // Add BreakPoint
    void Add(u32 em_address, bool temp = false);
    void Add(const TBreakPoint& bp);

    // Remove Breakpoint
    void Remove(u32 iAddress);
    void Clear();

    void DeleteByAddress(u32 Address);

private:
    TBreakPoints m_BreakPoints;
    u32 m_iBreakOnCount;
};
