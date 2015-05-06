// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <string>

#include "common/common_types.h"

class DebugInterface;

struct TBreakPoint
{
    u32  iAddress;
    bool bOn;
    bool bTemporary;
};

struct TMemCheck
{
    TMemCheck():
        StartAddress(0), EndAddress(0),
        bRange(false), OnRead(false), OnWrite(false),
        Log(false), Break(false), numHits(0)
    { }

    u32  StartAddress;
    u32  EndAddress;

    bool bRange;

    bool OnRead;
    bool OnWrite;

    bool Log;
    bool Break;

    u32  numHits;

    void Action(DebugInterface *dbg_interface, u32 iValue, u32 addr,
                bool write, int size, u32 pc);
};

// Code breakpoints.
class BreakPoints
{
public:
    typedef std::vector<TBreakPoint> TBreakPoints;
    typedef std::vector<std::string> TBreakPointsStr;

    const TBreakPoints& GetBreakPoints() { return m_BreakPoints; }

    TBreakPointsStr GetStrings() const;
    void AddFromStrings(const TBreakPointsStr& bps);

    // is address breakpoint
    bool IsAddressBreakPoint(u32 iAddress) const;
    bool IsTempBreakPoint(u32 iAddress) const;

    // Add BreakPoint
    void Add(u32 em_address, bool temp=false);
    void Add(const TBreakPoint& bp);

    // Remove Breakpoint
    void Remove(u32 iAddress);
    void Clear();

    void DeleteByAddress(u32 Address);

private:
    TBreakPoints m_BreakPoints;
    u32          m_iBreakOnCount;
};


// Memory breakpoints
class MemChecks
{
public:
    typedef std::vector<TMemCheck> TMemChecks;
    typedef std::vector<std::string> TMemChecksStr;

    TMemChecks m_MemChecks;

    const TMemChecks& GetMemChecks() { return m_MemChecks; }

    TMemChecksStr GetStrings() const;
    void AddFromStrings(const TMemChecksStr& mcs);

    void Add(const TMemCheck& rMemoryCheck);

    // memory breakpoint
    TMemCheck *GetMemCheck(u32 address);
    void Remove(u32 _Address);

    void Clear() { m_MemChecks.clear(); };
};
