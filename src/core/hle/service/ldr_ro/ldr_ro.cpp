// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/arm/arm_interface.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/service/ldr_ro/cro_helper.h"
#include "core/hle/service/ldr_ro/ldr_ro.h"
#include "core/hle/service/ldr_ro/memory_synchronizer.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace LDR_RO

namespace LDR_RO {

static const ResultCode ERROR_ALREADY_INITIALIZED =   // 0xD9612FF9
    ResultCode(ErrorDescription::AlreadyInitialized,         ErrorModule::RO, ErrorSummary::Internal,        ErrorLevel::Permanent);
static const ResultCode ERROR_NOT_INITIALIZED =       // 0xD9612FF8
    ResultCode(ErrorDescription::NotInitialized,             ErrorModule::RO, ErrorSummary::Internal,        ErrorLevel::Permanent);
static const ResultCode ERROR_BUFFER_TOO_SMALL =      // 0xE0E12C1F
    ResultCode(static_cast<ErrorDescription>(31),            ErrorModule::RO, ErrorSummary::InvalidArgument, ErrorLevel::Usage);
static const ResultCode ERROR_MISALIGNED_ADDRESS =    // 0xD9012FF1
    ResultCode(ErrorDescription::MisalignedAddress,          ErrorModule::RO, ErrorSummary::WrongArgument,   ErrorLevel::Permanent);
static const ResultCode ERROR_MISALIGNED_SIZE =       // 0xD9012FF2
    ResultCode(ErrorDescription::MisalignedSize,             ErrorModule::RO, ErrorSummary::WrongArgument,   ErrorLevel::Permanent);
static const ResultCode ERROR_ILLEGAL_ADDRESS =       // 0xE1612C0F
    ResultCode(static_cast<ErrorDescription>(15),            ErrorModule::RO, ErrorSummary::Internal,        ErrorLevel::Usage);
static const ResultCode ERROR_INVALID_MEMORY_STATE =  // 0xD8A12C08
    ResultCode(static_cast<ErrorDescription>(8),             ErrorModule::RO, ErrorSummary::InvalidState,    ErrorLevel::Permanent);
static const ResultCode ERROR_NOT_LOADED =            // 0xD8A12C0D
    ResultCode(static_cast<ErrorDescription>(13),            ErrorModule::RO, ErrorSummary::InvalidState,    ErrorLevel::Permanent);
static const ResultCode ERROR_INVALID_DESCRIPTOR =    // 0xD9001830
    ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS, ErrorSummary::WrongArgument,   ErrorLevel::Permanent);

static MemorySynchronizer memory_synchronizer;

// TODO(wwylele): this should be in the per-client storage when we implement multi-process
static VAddr loaded_crs; ///< the virtual address of the static module

static bool VerifyBufferState(VAddr buffer_ptr, u32 size) {
    auto vma = Kernel::g_current_process->vm_manager.FindVMA(buffer_ptr);
    return vma != Kernel::g_current_process->vm_manager.vma_map.end()
        && vma->second.base + vma->second.size >= buffer_ptr + size
        && vma->second.permissions == Kernel::VMAPermission::ReadWrite
        && vma->second.meminfo_state == Kernel::MemoryState::Private;
}

/**
 * LDR_RO::Initialize service function
 *  Inputs:
 *      0 : 0x000100C2
 *      1 : CRS buffer pointer
 *      2 : CRS Size
 *      3 : Process memory address where the CRS will be mapped
 *      4 : handle translation descriptor (zero)
 *      5 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    VAddr crs_buffer_ptr = cmd_buff[1];
    u32 crs_size         = cmd_buff[2];
    VAddr crs_address    = cmd_buff[3];
    u32 descriptor       = cmd_buff[4];
    u32 process          = cmd_buff[5];

    LOG_DEBUG(Service_LDR, "called, crs_buffer_ptr=0x%08X, crs_address=0x%08X, crs_size=0x%X, descriptor=0x%08X, process=0x%08X",
                crs_buffer_ptr, crs_address, crs_size, descriptor, process);

    if (descriptor != 0) {
        LOG_ERROR(Service_LDR, "IPC handle descriptor failed validation (0x%X)", descriptor);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ERROR_INVALID_DESCRIPTOR.raw;
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(1, 1, 0);

    if (loaded_crs != 0) {
        LOG_ERROR(Service_LDR, "Already initialized");
        cmd_buff[1] = ERROR_ALREADY_INITIALIZED.raw;
        return;
    }

    if (crs_size < CRO_HEADER_SIZE) {
        LOG_ERROR(Service_LDR, "CRS is too small");
        cmd_buff[1] = ERROR_BUFFER_TOO_SMALL.raw;
        return;
    }

    if (crs_buffer_ptr & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS original address is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_ADDRESS.raw;
        return;
    }

    if (crs_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS mapping address is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_ADDRESS.raw;
        return;
    }

    if (crs_size & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS size is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_SIZE.raw;
        return;
    }

    if (!VerifyBufferState(crs_buffer_ptr, crs_size)) {
        LOG_ERROR(Service_LDR, "CRS original buffer is in invalid state");
        cmd_buff[1] = ERROR_INVALID_MEMORY_STATE.raw;
        return;
    }

    if (crs_address < Memory::PROCESS_IMAGE_VADDR || crs_address + crs_size > Memory::PROCESS_IMAGE_VADDR_END) {
        LOG_ERROR(Service_LDR, "CRS mapping address is not in the process image region");
        cmd_buff[1] = ERROR_ILLEGAL_ADDRESS.raw;
        return;
    }

    ResultCode result = RESULT_SUCCESS;

    if (crs_buffer_ptr != crs_address) {
        // TODO(wwylele): should be memory aliasing
        std::shared_ptr<std::vector<u8>> crs_mem = std::make_shared<std::vector<u8>>(crs_size);
        Memory::ReadBlock(crs_buffer_ptr, crs_mem->data(), crs_size);
        result = Kernel::g_current_process->vm_manager.MapMemoryBlock(crs_address, crs_mem, 0, crs_size, Kernel::MemoryState::Code).Code();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error mapping memory block %08X", result.raw);
            cmd_buff[1] = result.raw;
            return;
        }

        result = Kernel::g_current_process->vm_manager.ReprotectRange(crs_address, crs_size, Kernel::VMAPermission::Read);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block %08X", result.raw);
            cmd_buff[1] = result.raw;
            return;
        }

        memory_synchronizer.AddMemoryBlock(crs_address, crs_buffer_ptr, crs_size);
    } else {
        // Do nothing if buffer_ptr == address
        // TODO(wwylele): verify this behaviour. This is only seen in the web browser app,
        //     and the actual behaviour is unclear. "Do nothing" is probably an incorrect implement.
        //     There is also a chance that another issue causes the app passing wrong arguments.
        LOG_WARNING(Service_LDR, "crs_buffer_ptr == crs_address (0x%08X)", crs_address);
    }

    CROHelper crs(crs_address);
    crs.InitCRS();

    result = crs.Rebase(0, crs_size, 0, 0, 0, 0, true);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing CRS 0x%08X", result.raw);
        cmd_buff[1] = result.raw;
        return;
    }

    memory_synchronizer.SynchronizeOriginalMemory();

    loaded_crs = crs_address;

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

/**
 * LDR_RO::LoadCRR service function
 *  Inputs:
 *      0 : 0x00020082
 *      1 : CRR buffer pointer
 *      2 : CRR Size
 *      3 : handle translation descriptor (zero)
 *      4 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void LoadCRR(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 crr_buffer_ptr = cmd_buff[1];
    u32 crr_size       = cmd_buff[2];
    u32 descriptor     = cmd_buff[3];
    u32 process        = cmd_buff[4];

    if (descriptor != 0) {
        LOG_ERROR(Service_LDR, "IPC handle descriptor failed validation (0x%X)", descriptor);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ERROR_INVALID_DESCRIPTOR.raw;
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(2, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_LDR, "(STUBBED) called, crr_buffer_ptr=0x%08X, crr_size=0x%08X, descriptor=0x%08X, process=0x%08X",
                crr_buffer_ptr, crr_size, descriptor, process);
}

/**
 * LDR_RO::UnloadCRR service function
 *  Inputs:
 *      0 : 0x00030042
 *      1 : CRR buffer pointer
 *      2 : handle translation descriptor (zero)
 *      3 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void UnloadCRR(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 crr_buffer_ptr = cmd_buff[1];
    u32 descriptor     = cmd_buff[2];
    u32 process        = cmd_buff[3];

    if (descriptor != 0) {
        LOG_ERROR(Service_LDR, "IPC handle descriptor failed validation (0x%X)", descriptor);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ERROR_INVALID_DESCRIPTOR.raw;
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(3, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_LDR, "(STUBBED) called, crr_buffer_ptr=0x%08X, descriptor=0x%08X, process=0x%08X",
                crr_buffer_ptr, descriptor, process);
}

/**
 * LDR_RO::LoadCRO service function
 *  Inputs:
 *      0 : 0x000402C2 (old) / 0x000902C2 (new)
 *      1 : CRO buffer pointer
 *      2 : memory address where the CRO will be mapped
 *      3 : CRO Size
 *      4 : .data segment buffer pointer
 *      5 : must be zero
 *      6 : .data segment buffer size
 *      7 : .bss segment buffer pointer
 *      8 : .bss segment buffer size
 *      9 : (bool) register CRO as auto-link module
 *     10 : fix level
 *     11 : CRR address (zero if use loaded CRR)
 *     12 : handle translation descriptor (zero)
 *     13 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : CRO fixed size
 *  Note:
 *      This service function has two versions. The function defined here is a
 *      unified one of two, with an additional parameter link_on_load_bug_fix.
 *      There is a dispatcher template below.
 */
static void LoadCRO(Service::Interface* self, bool link_on_load_bug_fix) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    VAddr cro_buffer_ptr       = cmd_buff[1];
    VAddr cro_address          = cmd_buff[2];
    u32 cro_size               = cmd_buff[3];
    VAddr data_segment_address = cmd_buff[4];
    u32 zero                   = cmd_buff[5];
    u32 data_segment_size      = cmd_buff[6];
    u32 bss_segment_address    = cmd_buff[7];
    u32 bss_segment_size       = cmd_buff[8];
    bool auto_link             = (cmd_buff[9] & 0xFF) != 0;
    u32 fix_level              = cmd_buff[10];
    VAddr crr_address          = cmd_buff[11];
    u32 descriptor             = cmd_buff[12];
    u32 process                = cmd_buff[13];

    LOG_DEBUG(Service_LDR, "called (%s), cro_buffer_ptr=0x%08X, cro_address=0x%08X, cro_size=0x%X, "
        "data_segment_address=0x%08X, zero=%d, data_segment_size=0x%X, bss_segment_address=0x%08X, bss_segment_size=0x%X, "
        "auto_link=%s, fix_level=%d, crr_address=0x%08X, descriptor=0x%08X, process=0x%08X",
        link_on_load_bug_fix ? "new" : "old", cro_buffer_ptr, cro_address, cro_size,
        data_segment_address, zero, data_segment_size, bss_segment_address, bss_segment_size,
        auto_link ? "true" : "false", fix_level, crr_address, descriptor, process
        );

    if (descriptor != 0) {
        LOG_ERROR(Service_LDR, "IPC handle descriptor failed validation (0x%X)", descriptor);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ERROR_INVALID_DESCRIPTOR.raw;
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(link_on_load_bug_fix ? 9 : 4, 2, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        cmd_buff[1] = ERROR_NOT_INITIALIZED.raw;
        return;
    }

    if (cro_size < CRO_HEADER_SIZE) {
        LOG_ERROR(Service_LDR, "CRO too small");
        cmd_buff[1] = ERROR_BUFFER_TOO_SMALL.raw;
        return;
    }

    if (cro_buffer_ptr & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO original address is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_ADDRESS.raw;
        return;
    }

    if (cro_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO mapping address is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_ADDRESS.raw;
        return;
    }

    if (cro_size & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO size is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_SIZE.raw;
        return;
    }

    if (!VerifyBufferState(cro_buffer_ptr, cro_size)) {
        LOG_ERROR(Service_LDR, "CRO original buffer is in invalid state");
        cmd_buff[1] = ERROR_INVALID_MEMORY_STATE.raw;
        return;
    }

    if (cro_address < Memory::PROCESS_IMAGE_VADDR
        || cro_address + cro_size > Memory::PROCESS_IMAGE_VADDR_END) {
        LOG_ERROR(Service_LDR, "CRO mapping address is not in the process image region");
        cmd_buff[1] = ERROR_ILLEGAL_ADDRESS.raw;
        return;
    }

    if (zero) {
        LOG_ERROR(Service_LDR, "Zero is not zero %d", zero);
        cmd_buff[1] = ResultCode(static_cast<ErrorDescription>(29), ErrorModule::RO, ErrorSummary::Internal, ErrorLevel::Usage).raw;
        return;
    }

    ResultCode result = RESULT_SUCCESS;

    if (cro_buffer_ptr != cro_address) {
        // TODO(wwylele): should be memory aliasing
        std::shared_ptr<std::vector<u8>> cro_mem = std::make_shared<std::vector<u8>>(cro_size);
        Memory::ReadBlock(cro_buffer_ptr, cro_mem->data(), cro_size);
        result = Kernel::g_current_process->vm_manager.MapMemoryBlock(cro_address, cro_mem, 0, cro_size, Kernel::MemoryState::Code).Code();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error mapping memory block %08X", result.raw);
            cmd_buff[1] = result.raw;
            return;
        }

        result = Kernel::g_current_process->vm_manager.ReprotectRange(cro_address, cro_size, Kernel::VMAPermission::Read);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block %08X", result.raw);
            Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
            cmd_buff[1] = result.raw;
            return;
        }

        memory_synchronizer.AddMemoryBlock(cro_address, cro_buffer_ptr, cro_size);
    } else {
        // Do nothing if buffer_ptr == address
        // TODO(wwylele): verify this behaviour.
        //     This is derived from the case of LoadCRS with buffer_ptr==address,
        //     and is never seen in any game. "Do nothing" is probably an incorrect implement.
        //     There is also a chance that this case is just prohibited.
        LOG_WARNING(Service_LDR, "cro_buffer_ptr == cro_address (0x%08X)", cro_address);
    }

    CROHelper cro(cro_address);

    result = cro.VerifyHash(cro_size, crr_address);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error verifying CRO in CRR %08X", result.raw);
        Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
        cmd_buff[1] = result.raw;
        return;
    }

    result = cro.Rebase(loaded_crs, cro_size, data_segment_address, data_segment_size, bss_segment_address, bss_segment_size, false);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing CRO %08X", result.raw);
        Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
        cmd_buff[1] = result.raw;
        return;
    }

    result = cro.Link(loaded_crs, link_on_load_bug_fix);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error linking CRO %08X", result.raw);
        Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
        cmd_buff[1] = result.raw;
        return;
    }

    cro.Register(loaded_crs, auto_link);

    u32 fix_size = cro.Fix(fix_level);

    memory_synchronizer.SynchronizeOriginalMemory();

    // TODO(wwylele): verify the behaviour when buffer_ptr == address
    if (cro_buffer_ptr != cro_address) {
        if (fix_size != cro_size) {
            result = Kernel::g_current_process->vm_manager.UnmapRange(cro_address + fix_size, cro_size - fix_size);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error unmapping memory block %08X", result.raw);
                Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
                cmd_buff[1] = result.raw;
                return;
            }
        }

        // Changes the block size
        memory_synchronizer.ResizeMemoryBlock(cro_address, cro_buffer_ptr, fix_size);
    }

    VAddr exe_begin;
    u32 exe_size;
    std::tie(exe_begin, exe_size) = cro.GetExecutablePages();
    if (exe_begin) {
        result = Kernel::g_current_process->vm_manager.ReprotectRange(exe_begin, exe_size, Kernel::VMAPermission::ReadExecute);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block %08X", result.raw);
            Kernel::g_current_process->vm_manager.UnmapRange(cro_address, fix_size);
            cmd_buff[1] = result.raw;
            return;
        }
    }

    Core::g_app_core->ClearInstructionCache();

    LOG_INFO(Service_LDR, "CRO \"%s\" loaded at 0x%08X, fixed_end=0x%08X",
        cro.ModuleName().data(), cro_address, cro_address+fix_size);

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = fix_size;
}

template <bool link_on_load_bug_fix>
static void LoadCRO(Service::Interface* self) {
    LoadCRO(self, link_on_load_bug_fix);
}

/**
 * LDR_RO::UnloadCRO service function
 *  Inputs:
 *      0 : 0x000500C2
 *      1 : mapped CRO pointer
 *      2 : zero? (RO service doesn't care)
 *      3 : original CRO pointer
 *      4 : handle translation descriptor (zero)
 *      5 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void UnloadCRO(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    VAddr cro_address      = cmd_buff[1];
    u32 zero               = cmd_buff[2];
    VAddr cro_buffer_ptr   = cmd_buff[3];
    u32 descriptor         = cmd_buff[4];
    u32 process            = cmd_buff[5];

    LOG_DEBUG(Service_LDR, "called, cro_address=0x%08X, zero=%d, cro_buffer_ptr=0x%08X, descriptor=0x%08X, process=0x%08X",
        cro_address, zero, cro_buffer_ptr, descriptor, process);

    if (descriptor != 0) {
        LOG_ERROR(Service_LDR, "IPC handle descriptor failed validation (0x%X)", descriptor);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ERROR_INVALID_DESCRIPTOR.raw;
        return;
    }

    CROHelper cro(cro_address);

    cmd_buff[0] = IPC::MakeHeader(5, 1, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        cmd_buff[1] = ERROR_NOT_INITIALIZED.raw;
        return;
    }

    if (cro_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_ADDRESS.raw;
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        cmd_buff[1] = ERROR_NOT_LOADED.raw;
        return;
    }

    LOG_INFO(Service_LDR, "Unloading CRO \"%s\"", cro.ModuleName().data());

    u32 fixed_size = cro.GetFixedSize();

    cro.Unregister(loaded_crs);

    ResultCode result = cro.Unlink(loaded_crs);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unlinking CRO %08X", result.raw);
        cmd_buff[1] = result.raw;
        return;
    }

    // If the module is not fixed, clears all external/internal relocations
    // to restore the state before loading, so that it can be loaded again(?)
    if (!cro.IsFixed()) {
        result = cro.ClearRelocations();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error clearing relocations %08X", result.raw);
            cmd_buff[1] = result.raw;
            return;
        }
    }

    cro.Unrebase(false);

    memory_synchronizer.SynchronizeOriginalMemory();

    // TODO(wwylele): verify the behaviour when buffer_ptr == address
    if (cro_address != cro_buffer_ptr) {
        result = Kernel::g_current_process->vm_manager.UnmapRange(cro_address, fixed_size);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error unmapping CRO %08X", result.raw);
        }
        memory_synchronizer.RemoveMemoryBlock(cro_address, cro_buffer_ptr);
    }

    Core::g_app_core->ClearInstructionCache();

    cmd_buff[1] = result.raw;
}

/**
 * LDR_RO::LinkCRO service function
 *  Inputs:
 *      0 : 0x00060042
 *      1 : mapped CRO pointer
 *      2 : handle translation descriptor (zero)
 *      3 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void LinkCRO(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    VAddr cro_address = cmd_buff[1];
    u32 descriptor    = cmd_buff[2];
    u32 process       = cmd_buff[3];

    LOG_DEBUG(Service_LDR, "called, cro_address=0x%08X, descriptor=0x%08X, process=0x%08X",
        cro_address, descriptor, process);

    if (descriptor != 0) {
        LOG_ERROR(Service_LDR, "IPC handle descriptor failed validation (0x%X)", descriptor);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ERROR_INVALID_DESCRIPTOR.raw;
        return;
    }

    CROHelper cro(cro_address);

    cmd_buff[0] = IPC::MakeHeader(6, 1, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        cmd_buff[1] = ERROR_NOT_INITIALIZED.raw;
        return;
    }

    if (cro_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_ADDRESS.raw;
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        cmd_buff[1] = ERROR_NOT_LOADED.raw;
        return;
    }

    LOG_INFO(Service_LDR, "Linking CRO \"%s\"", cro.ModuleName().data());

    ResultCode result = cro.Link(loaded_crs, false);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error linking CRO %08X", result.raw);
    }

    memory_synchronizer.SynchronizeOriginalMemory();
    Core::g_app_core->ClearInstructionCache();

    cmd_buff[1] = result.raw;
}

/**
 * LDR_RO::UnlinkCRO service function
 *  Inputs:
 *      0 : 0x00070042
 *      1 : mapped CRO pointer
 *      2 : handle translation descriptor (zero)
 *      3 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void UnlinkCRO(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    VAddr cro_address = cmd_buff[1];
    u32 descriptor    = cmd_buff[2];
    u32 process       = cmd_buff[3];

    LOG_DEBUG(Service_LDR, "called, cro_address=0x%08X, descriptor=0x%08X, process=0x%08X",
        cro_address, descriptor, process);

    if (descriptor != 0) {
        LOG_ERROR(Service_LDR, "IPC handle descriptor failed validation (0x%X)", descriptor);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ERROR_INVALID_DESCRIPTOR.raw;
        return;
    }

    CROHelper cro(cro_address);

    cmd_buff[0] = IPC::MakeHeader(7, 1, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        cmd_buff[1] = ERROR_NOT_INITIALIZED.raw;
        return;
    }

    if (cro_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        cmd_buff[1] = ERROR_MISALIGNED_ADDRESS.raw;
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        cmd_buff[1] = ERROR_NOT_LOADED.raw;
        return;
    }

    LOG_INFO(Service_LDR, "Unlinking CRO \"%s\"", cro.ModuleName().data());

    ResultCode result = cro.Unlink(loaded_crs);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unlinking CRO %08X", result.raw);
    }

    memory_synchronizer.SynchronizeOriginalMemory();
    Core::g_app_core->ClearInstructionCache();

    cmd_buff[1] = result.raw;
}

/**
 * LDR_RO::Shutdown service function
 *  Inputs:
 *      0 : 0x00080042
 *      1 : original CRS buffer pointer
 *      2 : handle translation descriptor (zero)
 *      3 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Shutdown(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    VAddr crs_buffer_ptr = cmd_buff[1];
    u32 descriptor       = cmd_buff[2];
    u32 process          = cmd_buff[3];

    LOG_DEBUG(Service_LDR, "called, crs_buffer_ptr=0x%08X, descriptor=0x%08X, process=0x%08X",
        crs_buffer_ptr, descriptor, process);

    if (descriptor != 0) {
        LOG_ERROR(Service_LDR, "IPC handle descriptor failed validation (0x%X)", descriptor);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ERROR_INVALID_DESCRIPTOR.raw;
        return;
    }

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        cmd_buff[1] = ERROR_NOT_INITIALIZED.raw;
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(8, 1, 0);

    CROHelper crs(loaded_crs);
    crs.Unrebase(true);

    memory_synchronizer.SynchronizeOriginalMemory();

    ResultCode result = RESULT_SUCCESS;

    // TODO(wwylele): verify the behaviour when buffer_ptr == address
    if (loaded_crs != crs_buffer_ptr) {
        result = Kernel::g_current_process->vm_manager.UnmapRange(loaded_crs, crs.GetFileSize());
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error unmapping CRS %08X", result.raw);
        }
        memory_synchronizer.RemoveMemoryBlock(loaded_crs, crs_buffer_ptr);
    }

    loaded_crs = 0;
    cmd_buff[1] = result.raw;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C2, Initialize,            "Initialize"},
    {0x00020082, LoadCRR,               "LoadCRR"},
    {0x00030042, UnloadCRR,             "UnloadCRR"},
    {0x000402C2, LoadCRO<false>,        "LoadCRO"},
    {0x000500C2, UnloadCRO,             "UnloadCRO"},
    {0x00060042, LinkCRO,               "LinkCRO"},
    {0x00070042, UnlinkCRO,             "UnlinkCRO"},
    {0x00080042, Shutdown,              "Shutdown"},
    {0x000902C2, LoadCRO<true>,         "LoadCRO_New"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);

    loaded_crs = 0;
    memory_synchronizer.Clear();
}

} // namespace
