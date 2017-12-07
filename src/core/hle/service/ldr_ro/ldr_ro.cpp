// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/service/ldr_ro/cro_helper.h"
#include "core/hle/service/ldr_ro/ldr_ro.h"
#include "core/hle/service/ldr_ro/memory_synchronizer.h"

namespace Service {
namespace LDR {

static const ResultCode ERROR_ALREADY_INITIALIZED = // 0xD9612FF9
    ResultCode(ErrorDescription::AlreadyInitialized, ErrorModule::RO, ErrorSummary::Internal,
               ErrorLevel::Permanent);
static const ResultCode ERROR_NOT_INITIALIZED = // 0xD9612FF8
    ResultCode(ErrorDescription::NotInitialized, ErrorModule::RO, ErrorSummary::Internal,
               ErrorLevel::Permanent);
static const ResultCode ERROR_BUFFER_TOO_SMALL = // 0xE0E12C1F
    ResultCode(static_cast<ErrorDescription>(31), ErrorModule::RO, ErrorSummary::InvalidArgument,
               ErrorLevel::Usage);
static const ResultCode ERROR_MISALIGNED_ADDRESS = // 0xD9012FF1
    ResultCode(ErrorDescription::MisalignedAddress, ErrorModule::RO, ErrorSummary::WrongArgument,
               ErrorLevel::Permanent);
static const ResultCode ERROR_MISALIGNED_SIZE = // 0xD9012FF2
    ResultCode(ErrorDescription::MisalignedSize, ErrorModule::RO, ErrorSummary::WrongArgument,
               ErrorLevel::Permanent);
static const ResultCode ERROR_ILLEGAL_ADDRESS = // 0xE1612C0F
    ResultCode(static_cast<ErrorDescription>(15), ErrorModule::RO, ErrorSummary::Internal,
               ErrorLevel::Usage);
static const ResultCode ERROR_INVALID_MEMORY_STATE = // 0xD8A12C08
    ResultCode(static_cast<ErrorDescription>(8), ErrorModule::RO, ErrorSummary::InvalidState,
               ErrorLevel::Permanent);
static const ResultCode ERROR_NOT_LOADED = // 0xD8A12C0D
    ResultCode(static_cast<ErrorDescription>(13), ErrorModule::RO, ErrorSummary::InvalidState,
               ErrorLevel::Permanent);

static MemorySynchronizer memory_synchronizer;

// TODO(wwylele): this should be in the per-client storage when we implement multi-process
static VAddr loaded_crs; ///< the virtual address of the static module

static bool VerifyBufferState(VAddr buffer_ptr, u32 size) {
    auto vma = Kernel::g_current_process->vm_manager.FindVMA(buffer_ptr);
    return vma != Kernel::g_current_process->vm_manager.vma_map.end() &&
           vma->second.base + vma->second.size >= buffer_ptr + size &&
           vma->second.permissions == Kernel::VMAPermission::ReadWrite &&
           vma->second.meminfo_state == Kernel::MemoryState::Private;
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
static void Initialize(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x01, 3, 2);
    VAddr crs_buffer_ptr = rp.Pop<u32>();
    u32 crs_size = rp.Pop<u32>();
    VAddr crs_address = rp.Pop<u32>();
    // TODO (wwylele): RO service checks the descriptor here and return error 0xD9001830 for
    // incorrect descriptor. This error return should be probably built in IPC::RequestParser.
    // All other service functions below have the same issue.
    Kernel::Handle process = rp.PopHandle();

    LOG_DEBUG(Service_LDR,
              "called, crs_buffer_ptr=0x%08X, crs_address=0x%08X, crs_size=0x%X, process=0x%08X",
              crs_buffer_ptr, crs_address, crs_size, process);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (loaded_crs != 0) {
        LOG_ERROR(Service_LDR, "Already initialized");
        rb.Push(ERROR_ALREADY_INITIALIZED);
        return;
    }

    if (crs_size < CRO_HEADER_SIZE) {
        LOG_ERROR(Service_LDR, "CRS is too small");
        rb.Push(ERROR_BUFFER_TOO_SMALL);
        return;
    }

    if (crs_buffer_ptr & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS original address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (crs_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS mapping address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (crs_size & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRS size is not aligned");
        rb.Push(ERROR_MISALIGNED_SIZE);
        return;
    }

    if (!VerifyBufferState(crs_buffer_ptr, crs_size)) {
        LOG_ERROR(Service_LDR, "CRS original buffer is in invalid state");
        rb.Push(ERROR_INVALID_MEMORY_STATE);
        return;
    }

    if (crs_address < Memory::PROCESS_IMAGE_VADDR ||
        crs_address + crs_size > Memory::PROCESS_IMAGE_VADDR_END) {
        LOG_ERROR(Service_LDR, "CRS mapping address is not in the process image region");
        rb.Push(ERROR_ILLEGAL_ADDRESS);
        return;
    }

    ResultCode result = RESULT_SUCCESS;

    if (crs_buffer_ptr != crs_address) {
        // TODO(wwylele): should be memory aliasing
        std::shared_ptr<std::vector<u8>> crs_mem = std::make_shared<std::vector<u8>>(crs_size);
        Memory::ReadBlock(crs_buffer_ptr, crs_mem->data(), crs_size);
        result = Kernel::g_current_process->vm_manager
                     .MapMemoryBlock(crs_address, crs_mem, 0, crs_size, Kernel::MemoryState::Code)
                     .Code();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error mapping memory block %08X", result.raw);
            rb.Push(result);
            return;
        }

        result = Kernel::g_current_process->vm_manager.ReprotectRange(crs_address, crs_size,
                                                                      Kernel::VMAPermission::Read);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block %08X", result.raw);
            rb.Push(result);
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
        rb.Push(result);
        return;
    }

    memory_synchronizer.SynchronizeOriginalMemory();

    loaded_crs = crs_address;

    rb.Push(RESULT_SUCCESS);
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
static void LoadCRR(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x02, 2, 2);
    VAddr crr_buffer_ptr = rp.Pop<u32>();
    u32 crr_size = rp.Pop<u32>();
    Kernel::Handle process = rp.PopHandle();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_LDR,
                "(STUBBED) called, crr_buffer_ptr=0x%08X, crr_size=0x%08X, process=0x%08X",
                crr_buffer_ptr, crr_size, process);
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
static void UnloadCRR(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x03, 1, 2);
    u32 crr_buffer_ptr = rp.Pop<u32>();
    Kernel::Handle process = rp.PopHandle();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_LDR, "(STUBBED) called, crr_buffer_ptr=0x%08X, process=0x%08X",
                crr_buffer_ptr, process);
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
static void LoadCRO(Interface* self, bool link_on_load_bug_fix) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), link_on_load_bug_fix ? 0x09 : 0x04, 11, 2);
    VAddr cro_buffer_ptr = rp.Pop<u32>();
    VAddr cro_address = rp.Pop<u32>();
    u32 cro_size = rp.Pop<u32>();
    VAddr data_segment_address = rp.Pop<u32>();
    u32 zero = rp.Pop<u32>();
    u32 data_segment_size = rp.Pop<u32>();
    u32 bss_segment_address = rp.Pop<u32>();
    u32 bss_segment_size = rp.Pop<u32>();
    bool auto_link = rp.Pop<bool>();
    u32 fix_level = rp.Pop<u32>();
    VAddr crr_address = rp.Pop<u32>();
    Kernel::Handle process = rp.PopHandle();

    LOG_DEBUG(Service_LDR, "called (%s), cro_buffer_ptr=0x%08X, cro_address=0x%08X, cro_size=0x%X, "
                           "data_segment_address=0x%08X, zero=%d, data_segment_size=0x%X, "
                           "bss_segment_address=0x%08X, bss_segment_size=0x%X, auto_link=%s, "
                           "fix_level=%d, crr_address=0x%08X, process=0x%08X",
              link_on_load_bug_fix ? "new" : "old", cro_buffer_ptr, cro_address, cro_size,
              data_segment_address, zero, data_segment_size, bss_segment_address, bss_segment_size,
              auto_link ? "true" : "false", fix_level, crr_address, process);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        rb.Push<u32>(0);
        return;
    }

    if (cro_size < CRO_HEADER_SIZE) {
        LOG_ERROR(Service_LDR, "CRO too small");
        rb.Push(ERROR_BUFFER_TOO_SMALL);
        rb.Push<u32>(0);
        return;
    }

    if (cro_buffer_ptr & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO original address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        rb.Push<u32>(0);
        return;
    }

    if (cro_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO mapping address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        rb.Push<u32>(0);
        return;
    }

    if (cro_size & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO size is not aligned");
        rb.Push(ERROR_MISALIGNED_SIZE);
        rb.Push<u32>(0);
        return;
    }

    if (!VerifyBufferState(cro_buffer_ptr, cro_size)) {
        LOG_ERROR(Service_LDR, "CRO original buffer is in invalid state");
        rb.Push(ERROR_INVALID_MEMORY_STATE);
        rb.Push<u32>(0);
        return;
    }

    if (cro_address < Memory::PROCESS_IMAGE_VADDR ||
        cro_address + cro_size > Memory::PROCESS_IMAGE_VADDR_END) {
        LOG_ERROR(Service_LDR, "CRO mapping address is not in the process image region");
        rb.Push(ERROR_ILLEGAL_ADDRESS);
        rb.Push<u32>(0);
        return;
    }

    if (zero) {
        LOG_ERROR(Service_LDR, "Zero is not zero %d", zero);
        rb.Push(ResultCode(static_cast<ErrorDescription>(29), ErrorModule::RO,
                           ErrorSummary::Internal, ErrorLevel::Usage));
        rb.Push<u32>(0);
        return;
    }

    ResultCode result = RESULT_SUCCESS;

    if (cro_buffer_ptr != cro_address) {
        // TODO(wwylele): should be memory aliasing
        std::shared_ptr<std::vector<u8>> cro_mem = std::make_shared<std::vector<u8>>(cro_size);
        Memory::ReadBlock(cro_buffer_ptr, cro_mem->data(), cro_size);
        result = Kernel::g_current_process->vm_manager
                     .MapMemoryBlock(cro_address, cro_mem, 0, cro_size, Kernel::MemoryState::Code)
                     .Code();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error mapping memory block %08X", result.raw);
            rb.Push(result);
            rb.Push<u32>(0);
            return;
        }

        result = Kernel::g_current_process->vm_manager.ReprotectRange(cro_address, cro_size,
                                                                      Kernel::VMAPermission::Read);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block %08X", result.raw);
            Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
            rb.Push(result);
            rb.Push<u32>(0);
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
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    result = cro.Rebase(loaded_crs, cro_size, data_segment_address, data_segment_size,
                        bss_segment_address, bss_segment_size, false);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing CRO %08X", result.raw);
        Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    result = cro.Link(loaded_crs, link_on_load_bug_fix);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error linking CRO %08X", result.raw);
        Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    cro.Register(loaded_crs, auto_link);

    u32 fix_size = cro.Fix(fix_level);

    memory_synchronizer.SynchronizeOriginalMemory();

    // TODO(wwylele): verify the behaviour when buffer_ptr == address
    if (cro_buffer_ptr != cro_address) {
        if (fix_size != cro_size) {
            result = Kernel::g_current_process->vm_manager.UnmapRange(cro_address + fix_size,
                                                                      cro_size - fix_size);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error unmapping memory block %08X", result.raw);
                Kernel::g_current_process->vm_manager.UnmapRange(cro_address, cro_size);
                rb.Push(result);
                rb.Push<u32>(0);
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
        result = Kernel::g_current_process->vm_manager.ReprotectRange(
            exe_begin, exe_size, Kernel::VMAPermission::ReadExecute);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block %08X", result.raw);
            Kernel::g_current_process->vm_manager.UnmapRange(cro_address, fix_size);
            rb.Push(result);
            rb.Push<u32>(0);
            return;
        }
    }

    Core::CPU().InvalidateCacheRange(cro_address, cro_size);

    LOG_INFO(Service_LDR, "CRO \"%s\" loaded at 0x%08X, fixed_end=0x%08X", cro.ModuleName().data(),
             cro_address, cro_address + fix_size);

    rb.Push(RESULT_SUCCESS, fix_size);
}

template <bool link_on_load_bug_fix>
static void LoadCRO(Interface* self) {
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
static void UnloadCRO(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x05, 3, 2);
    VAddr cro_address = rp.Pop<u32>();
    u32 zero = rp.Pop<u32>();
    VAddr cro_buffer_ptr = rp.Pop<u32>();
    Kernel::Handle process = rp.PopHandle();

    LOG_DEBUG(Service_LDR,
              "called, cro_address=0x%08X, zero=%d, cro_buffer_ptr=0x%08X, process=0x%08X",
              cro_address, zero, cro_buffer_ptr, process);

    CROHelper cro(cro_address);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

    if (cro_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        rb.Push(ERROR_NOT_LOADED);
        return;
    }

    LOG_INFO(Service_LDR, "Unloading CRO \"%s\"", cro.ModuleName().data());

    u32 fixed_size = cro.GetFixedSize();

    cro.Unregister(loaded_crs);

    ResultCode result = cro.Unlink(loaded_crs);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unlinking CRO %08X", result.raw);
        rb.Push(result);
        return;
    }

    // If the module is not fixed, clears all external/internal relocations
    // to restore the state before loading, so that it can be loaded again(?)
    if (!cro.IsFixed()) {
        result = cro.ClearRelocations();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error clearing relocations %08X", result.raw);
            rb.Push(result);
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

    Core::CPU().InvalidateCacheRange(cro_address, fixed_size);

    rb.Push(result);
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
static void LinkCRO(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x06, 1, 2);
    VAddr cro_address = rp.Pop<u32>();
    Kernel::Handle process = rp.PopHandle();

    LOG_DEBUG(Service_LDR, "called, cro_address=0x%08X, process=0x%08X", cro_address, process);

    CROHelper cro(cro_address);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

    if (cro_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        rb.Push(ERROR_NOT_LOADED);
        return;
    }

    LOG_INFO(Service_LDR, "Linking CRO \"%s\"", cro.ModuleName().data());

    ResultCode result = cro.Link(loaded_crs, false);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error linking CRO %08X", result.raw);
    }

    memory_synchronizer.SynchronizeOriginalMemory();

    rb.Push(result);
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
static void UnlinkCRO(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x07, 1, 2);
    VAddr cro_address = rp.Pop<u32>();
    Kernel::Handle process = rp.PopHandle();

    LOG_DEBUG(Service_LDR, "called, cro_address=0x%08X, process=0x%08X", cro_address, process);

    CROHelper cro(cro_address);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

    if (cro_address & Memory::PAGE_MASK) {
        LOG_ERROR(Service_LDR, "CRO address is not aligned");
        rb.Push(ERROR_MISALIGNED_ADDRESS);
        return;
    }

    if (!cro.IsLoaded()) {
        LOG_ERROR(Service_LDR, "Invalid or not loaded CRO");
        rb.Push(ERROR_NOT_LOADED);
        return;
    }

    LOG_INFO(Service_LDR, "Unlinking CRO \"%s\"", cro.ModuleName().data());

    ResultCode result = cro.Unlink(loaded_crs);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unlinking CRO %08X", result.raw);
    }

    memory_synchronizer.SynchronizeOriginalMemory();

    rb.Push(result);
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
static void Shutdown(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x08, 1, 2);
    VAddr crs_buffer_ptr = rp.Pop<u32>();
    Kernel::Handle process = rp.PopHandle();

    LOG_DEBUG(Service_LDR, "called, crs_buffer_ptr=0x%08X, process=0x%08X", crs_buffer_ptr,
              process);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

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
    rb.Push(result);
}

const Interface::FunctionInfo FunctionTable[] = {
    // clang-format off
    {0x000100C2, Initialize, "Initialize"},
    {0x00020082, LoadCRR, "LoadCRR"},
    {0x00030042, UnloadCRR, "UnloadCRR"},
    {0x000402C2, LoadCRO<false>, "LoadCRO"},
    {0x000500C2, UnloadCRO, "UnloadCRO"},
    {0x00060042, LinkCRO, "LinkCRO"},
    {0x00070042, UnlinkCRO, "UnlinkCRO"},
    {0x00080042, Shutdown, "Shutdown"},
    {0x000902C2, LoadCRO<true>, "LoadCRO_New"},
    // clang-format on
};

LDR_RO::LDR_RO() {
    Register(FunctionTable);

    loaded_crs = 0;
    memory_synchronizer.Clear();
}

} // namespace LDR
} // namespace Service
