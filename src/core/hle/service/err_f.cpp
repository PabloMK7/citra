// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/err_f.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace ERR_F

namespace ERR_F {

enum {
    ErrSpecifier0 = 0,
    ErrSpecifier1 = 1,
    ErrSpecifier3 = 3,
    ErrSpecifier4 = 4,
};

// This is used instead of ResultCode from result.h
// because we can't have non-trivial data members in unions.
union RSL {
    u32 raw;

    BitField<0, 10, u32> description;
    BitField<10, 8, u32> module;
    BitField<21, 6, u32> summary;
    BitField<27, 5, u32> level;
};

union ErrInfo {
    u8 specifier;

    struct {
        u8 specifier;                // 0x0
        u8 rev_high;                 // 0x1
        u16 rev_low;                 // 0x2
        RSL result_code;             // 0x4
        u32 address;                 // 0x8
        INSERT_PADDING_BYTES(4);     // 0xC
        u32 pid_low;                 // 0x10
        u32 pid_high;                // 0x14
        u32 aid_low;                 // 0x18
        u32 aid_high;                // 0x1C
    } errtype1;

    struct {
        u8 specifier;                // 0x0
        u8 rev_high;                 // 0x1
        u16 rev_low;                 // 0x2
        INSERT_PADDING_BYTES(0xC);   // 0x4
        u32 pid_low;                 // 0x10
        u32 pid_high;                // 0x14
        u32 aid_low;                 // 0x18
        u32 aid_high;                // 0x1C
        u8 error_type;               // 0x20
        INSERT_PADDING_BYTES(3);     // 0x21
        u32 fault_status_reg;        // 0x24
        u32 fault_addr;              // 0x28
        u32 fpexc;                   // 0x2C
        u32 finst;                   // 0x30
        u32 finst2;                  // 0x34
        INSERT_PADDING_BYTES(0x34);  // 0x38
        u32 sp;                      // 0x6C
        u32 pc;                      // 0x70
        u32 lr;                      // 0x74
        u32 cpsr;                    // 0x78
    } errtype3;

    struct {
        u8 specifier;                // 0x0
        u8 rev_high;                 // 0x1
        u16 rev_low;                 // 0x2
        RSL result_code;             // 0x4
        INSERT_PADDING_BYTES(8);     // 0x8
        u32 pid_low;                 // 0x10
        u32 pid_high;                // 0x14
        u32 aid_low;                 // 0x18
        u32 aid_high;                // 0x1C
        char debug_string1[0x2E];    // 0x20
        char debug_string2[0x2E];    // 0x4E
    } errtype4;
};

enum {
    PrefetchAbort = 0,
    DataAbort     = 1,
    UndefInstr    = 2,
    VectorFP      = 3
};

static std::string GetErrInfo3Type(u8 type_code) {
    switch (type_code) {
    case PrefetchAbort: return "Prefetch Abort";
    case DataAbort:     return "Data Abort";
    case UndefInstr:    return "Undefined Instruction";
    case VectorFP:      return "Vector Floating Point";
    default: return "unknown";
    }
}

static void ThrowFatalError(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    LOG_CRITICAL(Service_ERR, "Fatal error!");
    const ErrInfo* errinfo = reinterpret_cast<ErrInfo*>(&cmd_buff[1]);

    switch (errinfo->specifier) {
    case ErrSpecifier0:
    case ErrSpecifier1:
    {
        const auto& errtype = errinfo->errtype1;
        LOG_CRITICAL(Service_ERR, "PID: 0x%08X_0x%08X", errtype.pid_low, errtype.pid_high);
        LOG_CRITICAL(Service_ERR, "REV: %d", errtype.rev_low | (errtype.rev_high << 16));
        LOG_CRITICAL(Service_ERR, "AID: 0x%08X_0x%08X", errtype.aid_low, errtype.aid_high);
        LOG_CRITICAL(Service_ERR, "ADR: 0x%08X", errtype.address);

        LOG_CRITICAL(Service_ERR, "RSL: 0x%08X", errtype.result_code.raw);
        LOG_CRITICAL(Service_ERR, "  Level: %u",   errtype.result_code.level.Value());
        LOG_CRITICAL(Service_ERR, "  Summary: %u", errtype.result_code.summary.Value());
        LOG_CRITICAL(Service_ERR, "  Module: %u",  errtype.result_code.module.Value());
        LOG_CRITICAL(Service_ERR, "  Desc: %u",    errtype.result_code.description.Value());
        break;
    }

    case ErrSpecifier3:
    {
        const auto& errtype = errinfo->errtype3;
        LOG_CRITICAL(Service_ERR, "PID: 0x%08X_0x%08X", errtype.pid_low, errtype.pid_high);
        LOG_CRITICAL(Service_ERR, "REV: %d", errtype.rev_low | (errtype.rev_high << 16));
        LOG_CRITICAL(Service_ERR, "AID: 0x%08X_0x%08X", errtype.aid_low, errtype.aid_high);
        LOG_CRITICAL(Service_ERR, "TYPE: %s", GetErrInfo3Type(errtype.error_type).c_str());

        LOG_CRITICAL(Service_ERR, "PC: 0x%08X", errtype.pc);
        LOG_CRITICAL(Service_ERR, "LR: 0x%08X", errtype.lr);
        LOG_CRITICAL(Service_ERR, "SP: 0x%08X", errtype.sp);
        LOG_CRITICAL(Service_ERR, "CPSR: 0x%08X", errtype.cpsr);

        switch (errtype.error_type) {
        case PrefetchAbort:
        case DataAbort:
            LOG_CRITICAL(Service_ERR, "Fault Address: 0x%08X", errtype.fault_addr);
            LOG_CRITICAL(Service_ERR, "Fault Status Register: 0x%08X", errtype.fault_status_reg);
            break;
        case VectorFP:
            LOG_CRITICAL(Service_ERR, "FPEXC: 0x%08X", errtype.fpexc);
            LOG_CRITICAL(Service_ERR, "FINST: 0x%08X", errtype.finst);
            LOG_CRITICAL(Service_ERR, "FINST2: 0x%08X", errtype.finst2);
            break;
        }
        break;
    }

    case ErrSpecifier4:
    {
        const auto& errtype = errinfo->errtype4;
        LOG_CRITICAL(Service_ERR, "PID: 0x%08X_0x%08X", errtype.pid_low, errtype.pid_high);
        LOG_CRITICAL(Service_ERR, "REV: %d", errtype.rev_low | (errtype.rev_high << 16));
        LOG_CRITICAL(Service_ERR, "AID: 0x%08X_0x%08X", errtype.aid_low, errtype.aid_high);

        LOG_CRITICAL(Service_ERR, "RSL: 0x%08X", errtype.result_code.raw);
        LOG_CRITICAL(Service_ERR, "  Level: %u",   errtype.result_code.level.Value());
        LOG_CRITICAL(Service_ERR, "  Summary: %u", errtype.result_code.summary.Value());
        LOG_CRITICAL(Service_ERR, "  Module: %u",  errtype.result_code.module.Value());
        LOG_CRITICAL(Service_ERR, "  Desc: %u",    errtype.result_code.description.Value());

        LOG_CRITICAL(Service_ERR, "%s", errtype.debug_string1);
        LOG_CRITICAL(Service_ERR, "%s", errtype.debug_string2);
        break;
    }
    }

    cmd_buff[1] = 0; // No error
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010800, ThrowFatalError,           "ThrowFatalError"}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
