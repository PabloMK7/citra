// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/debug_interface.h"
#include "common/break_points.h"
#include "common/logging/log.h"

#include <sstream>
#include <algorithm>

bool BreakPoints::IsAddressBreakPoint(u32 iAddress) const
{
    auto cond = [&iAddress](const TBreakPoint& bp) { return bp.iAddress == iAddress; };
    auto it   = std::find_if(m_BreakPoints.begin(), m_BreakPoints.end(), cond);
    return it != m_BreakPoints.end();
}

bool BreakPoints::IsTempBreakPoint(u32 iAddress) const
{
    auto cond = [&iAddress](const TBreakPoint& bp) { return bp.iAddress == iAddress && bp.bTemporary; };
    auto it   = std::find_if(m_BreakPoints.begin(), m_BreakPoints.end(), cond);
    return it != m_BreakPoints.end();
}

BreakPoints::TBreakPointsStr BreakPoints::GetStrings() const
{
    TBreakPointsStr bps;
    for (auto breakpoint : m_BreakPoints)
    {
        if (!breakpoint.bTemporary)
        {
            std::stringstream bp;
            bp << std::hex << breakpoint.iAddress << " " << (breakpoint.bOn ? "n" : "");
            bps.push_back(bp.str());
        }
    }

    return bps;
}

void BreakPoints::AddFromStrings(const TBreakPointsStr& bps)
{
    for (auto bps_item : bps)
    {
        TBreakPoint bp;
        std::stringstream bpstr;
        bpstr << std::hex << bps_item;
        bpstr >> bp.iAddress;
        bp.bOn = bps_item.find("n") != bps_item.npos;
        bp.bTemporary = false;
        Add(bp);
    }
}

void BreakPoints::Add(const TBreakPoint& bp)
{
    if (!IsAddressBreakPoint(bp.iAddress))
    {
        m_BreakPoints.push_back(bp);
        //if (jit)
        //    jit->GetBlockCache()->InvalidateICache(bp.iAddress, 4);
    }
}

void BreakPoints::Add(u32 em_address, bool temp)
{
    if (!IsAddressBreakPoint(em_address)) // only add new addresses
    {
        TBreakPoint pt; // breakpoint settings
        pt.bOn = true;
        pt.bTemporary = temp;
        pt.iAddress = em_address;

        m_BreakPoints.push_back(pt);

        //if (jit)
        //    jit->GetBlockCache()->InvalidateICache(em_address, 4);
    }
}

void BreakPoints::Remove(u32 em_address)
{
    auto cond = [&em_address](const TBreakPoint& bp) { return bp.iAddress == em_address; };
    auto it   = std::find_if(m_BreakPoints.begin(), m_BreakPoints.end(), cond);
    if (it != m_BreakPoints.end())
        m_BreakPoints.erase(it);
}

void BreakPoints::Clear()
{
    //if (jit)
    //{
    //    std::for_each(m_BreakPoints.begin(), m_BreakPoints.end(),
    //        [](const TBreakPoint& bp)
    //        {
    //            jit->GetBlockCache()->InvalidateICache(bp.iAddress, 4);
    //        }
    //    );
    //}

    m_BreakPoints.clear();
}

MemChecks::TMemChecksStr MemChecks::GetStrings() const
{
    TMemChecksStr mcs;
    for (auto memcheck : m_MemChecks)
    {
        std::stringstream mc;
        mc << std::hex << memcheck.StartAddress;
        mc << " " << (memcheck.bRange ? memcheck.EndAddress : memcheck.StartAddress) << " "
            << (memcheck.bRange  ? "n" : "")
            << (memcheck.OnRead  ? "r" : "")
            << (memcheck.OnWrite ? "w" : "")
            << (memcheck.Log     ? "l" : "")
            << (memcheck.Break   ? "p" : "");
        mcs.push_back(mc.str());
    }

    return mcs;
}

void MemChecks::AddFromStrings(const TMemChecksStr& mcs)
{
    for (auto mcs_item : mcs)
    {
        TMemCheck mc;
        std::stringstream mcstr;
        mcstr << std::hex << mcs_item;
        mcstr >> mc.StartAddress;
        mc.bRange   = mcs_item.find("n") != mcs_item.npos;
        mc.OnRead   = mcs_item.find("r") != mcs_item.npos;
        mc.OnWrite  = mcs_item.find("w") != mcs_item.npos;
        mc.Log      = mcs_item.find("l") != mcs_item.npos;
        mc.Break    = mcs_item.find("p") != mcs_item.npos;
        if (mc.bRange)
            mcstr >> mc.EndAddress;
        else
            mc.EndAddress = mc.StartAddress;
        Add(mc);
    }
}

void MemChecks::Add(const TMemCheck& rMemoryCheck)
{
    if (GetMemCheck(rMemoryCheck.StartAddress) == 0)
        m_MemChecks.push_back(rMemoryCheck);
}

void MemChecks::Remove(u32 Address)
{
    auto cond = [&Address](const TMemCheck& mc) { return mc.StartAddress == Address; };
    auto it   = std::find_if(m_MemChecks.begin(), m_MemChecks.end(), cond);
    if (it != m_MemChecks.end())
        m_MemChecks.erase(it);
}

TMemCheck *MemChecks::GetMemCheck(u32 address)
{
    for (auto i = m_MemChecks.begin(); i != m_MemChecks.end(); ++i)
    {
        if (i->bRange)
        {
            if (address >= i->StartAddress && address <= i->EndAddress)
                return &(*i);
        }
        else if (i->StartAddress == address)
            return &(*i);
    }

    // none found
    return 0;
}

void TMemCheck::Action(DebugInterface *debug_interface, u32 iValue, u32 addr,
                        bool write, int size, u32 pc)
{
    if ((write && OnWrite) || (!write && OnRead))
    {
        if (Log)
        {
            LOG_DEBUG(Debug_Breakpoint, "CHK %08x (%s) %s%i %0*x at %08x (%s)",
                pc, debug_interface->getDescription(pc).c_str(),
                write ? "Write" : "Read", size*8, size*2, iValue, addr,
                debug_interface->getDescription(addr).c_str()
                );
        }
        if (Break)
            debug_interface->breakNow();
    }
}
