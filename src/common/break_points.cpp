// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <sstream>
#include "common/break_points.h"
#include "common/logging/log.h"

bool BreakPoints::IsAddressBreakPoint(u32 iAddress) const {
    auto cond = [&iAddress](const TBreakPoint& bp) { return bp.iAddress == iAddress; };
    auto it = std::find_if(m_BreakPoints.begin(), m_BreakPoints.end(), cond);
    return it != m_BreakPoints.end();
}

bool BreakPoints::IsTempBreakPoint(u32 iAddress) const {
    auto cond = [&iAddress](const TBreakPoint& bp) {
        return bp.iAddress == iAddress && bp.bTemporary;
    };
    auto it = std::find_if(m_BreakPoints.begin(), m_BreakPoints.end(), cond);
    return it != m_BreakPoints.end();
}

BreakPoints::TBreakPointsStr BreakPoints::GetStrings() const {
    TBreakPointsStr bps;
    for (auto breakpoint : m_BreakPoints) {
        if (!breakpoint.bTemporary) {
            std::stringstream bp;
            bp << std::hex << breakpoint.iAddress << " " << (breakpoint.bOn ? "n" : "");
            bps.push_back(bp.str());
        }
    }

    return bps;
}

void BreakPoints::AddFromStrings(const TBreakPointsStr& bps) {
    for (auto bps_item : bps) {
        TBreakPoint bp;
        std::stringstream bpstr;
        bpstr << std::hex << bps_item;
        bpstr >> bp.iAddress;
        bp.bOn = bps_item.find("n") != bps_item.npos;
        bp.bTemporary = false;
        Add(bp);
    }
}

void BreakPoints::Add(const TBreakPoint& bp) {
    if (!IsAddressBreakPoint(bp.iAddress)) {
        m_BreakPoints.push_back(bp);
        // if (jit)
        //    jit->GetBlockCache()->InvalidateICache(bp.iAddress, 4);
    }
}

void BreakPoints::Add(u32 em_address, bool temp) {
    if (!IsAddressBreakPoint(em_address)) // only add new addresses
    {
        TBreakPoint pt; // breakpoint settings
        pt.bOn = true;
        pt.bTemporary = temp;
        pt.iAddress = em_address;

        m_BreakPoints.push_back(pt);

        // if (jit)
        //    jit->GetBlockCache()->InvalidateICache(em_address, 4);
    }
}

void BreakPoints::Remove(u32 em_address) {
    auto cond = [&em_address](const TBreakPoint& bp) { return bp.iAddress == em_address; };
    auto it = std::find_if(m_BreakPoints.begin(), m_BreakPoints.end(), cond);
    if (it != m_BreakPoints.end())
        m_BreakPoints.erase(it);
}

void BreakPoints::Clear() {
    // if (jit)
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
