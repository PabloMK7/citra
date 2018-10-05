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
#include "core/hle/service/ldr_ro/cro_helper.h"
#include "core/hle/service/ldr_ro/ldr_ro.h"

namespace Service::LDR {

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

static bool VerifyBufferState(Kernel::Process& process, VAddr buffer_ptr, u32 size) {
    auto vma = process.vm_manager.FindVMA(buffer_ptr);
    return vma != process.vm_manager.vma_map.end() &&
           vma->second.base + vma->second.size >= buffer_ptr + size &&
           vma->second.permissions == Kernel::VMAPermission::ReadWrite &&
           vma->second.meminfo_state == Kernel::MemoryState::Private;
}

void RO::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 3, 2);
    VAddr crs_buffer_ptr = rp.Pop<u32>();
    u32 crs_size = rp.Pop<u32>();
    VAddr crs_address = rp.Pop<u32>();
    // TODO (wwylele): RO service checks the descriptor here and return error 0xD9001830 for
    // incorrect descriptor. This error return should be probably built in IPC::RequestParser.
    // All other service functions below have the same issue.
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, crs_buffer_ptr=0x{:08X}, crs_address=0x{:08X}, crs_size=0x{:X}",
              crs_buffer_ptr, crs_address, crs_size);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs != 0) {
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

    if (!VerifyBufferState(*process, crs_buffer_ptr, crs_size)) {
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
        result = process->vm_manager
                     .MapMemoryBlock(crs_address, crs_mem, 0, crs_size, Kernel::MemoryState::Code)
                     .Code();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error mapping memory block {:08X}", result.raw);
            rb.Push(result);
            return;
        }

        result =
            process->vm_manager.ReprotectRange(crs_address, crs_size, Kernel::VMAPermission::Read);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block {:08X}", result.raw);
            rb.Push(result);
            return;
        }

        slot->memory_synchronizer.AddMemoryBlock(crs_address, crs_buffer_ptr, crs_size);
    } else {
        // Do nothing if buffer_ptr == address
        // TODO(wwylele): verify this behaviour. This is only seen in the web browser app,
        //     and the actual behaviour is unclear. "Do nothing" is probably an incorrect implement.
        //     There is also a chance that another issue causes the app passing wrong arguments.
        LOG_WARNING(Service_LDR, "crs_buffer_ptr == crs_address (0x{:08X})", crs_address);
    }

    CROHelper crs(crs_address);
    crs.InitCRS();

    result = crs.Rebase(0, crs_size, 0, 0, 0, 0, true);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing CRS 0x{:08X}", result.raw);
        rb.Push(result);
        return;
    }

    slot->memory_synchronizer.SynchronizeOriginalMemory(*process);

    slot->loaded_crs = crs_address;

    rb.Push(RESULT_SUCCESS);
}

void RO::LoadCRR(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 2, 2);
    VAddr crr_buffer_ptr = rp.Pop<u32>();
    u32 crr_size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_LDR, "(STUBBED) called, crr_buffer_ptr=0x{:08X}, crr_size=0x{:08X}",
                crr_buffer_ptr, crr_size);
}

void RO::UnloadCRR(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 1, 2);
    u32 crr_buffer_ptr = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_LDR, "(STUBBED) called, crr_buffer_ptr=0x{:08X}", crr_buffer_ptr);
}

void RO::LoadCRO(Kernel::HLERequestContext& ctx, bool link_on_load_bug_fix) {
    IPC::RequestParser rp(ctx, link_on_load_bug_fix ? 0x09 : 0x04, 11, 2);
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
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR,
              "called ({}), cro_buffer_ptr=0x{:08X}, cro_address=0x{:08X}, cro_size=0x{:X}, "
              "data_segment_address=0x{:08X}, zero={}, data_segment_size=0x{:X}, "
              "bss_segment_address=0x{:08X}, bss_segment_size=0x{:X}, auto_link={}, "
              "fix_level={}, crr_address=0x{:08X}",
              link_on_load_bug_fix ? "new" : "old", cro_buffer_ptr, cro_address, cro_size,
              data_segment_address, zero, data_segment_size, bss_segment_address, bss_segment_size,
              auto_link ? "true" : "false", fix_level, crr_address);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
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

    if (!VerifyBufferState(*process, cro_buffer_ptr, cro_size)) {
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
        LOG_ERROR(Service_LDR, "Zero is not zero {}", zero);
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
        result = process->vm_manager
                     .MapMemoryBlock(cro_address, cro_mem, 0, cro_size, Kernel::MemoryState::Code)
                     .Code();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error mapping memory block {:08X}", result.raw);
            rb.Push(result);
            rb.Push<u32>(0);
            return;
        }

        result =
            process->vm_manager.ReprotectRange(cro_address, cro_size, Kernel::VMAPermission::Read);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block {:08X}", result.raw);
            process->vm_manager.UnmapRange(cro_address, cro_size);
            rb.Push(result);
            rb.Push<u32>(0);
            return;
        }

        slot->memory_synchronizer.AddMemoryBlock(cro_address, cro_buffer_ptr, cro_size);
    } else {
        // Do nothing if buffer_ptr == address
        // TODO(wwylele): verify this behaviour.
        //     This is derived from the case of LoadCRS with buffer_ptr==address,
        //     and is never seen in any game. "Do nothing" is probably an incorrect implement.
        //     There is also a chance that this case is just prohibited.
        LOG_WARNING(Service_LDR, "cro_buffer_ptr == cro_address (0x{:08X})", cro_address);
    }

    CROHelper cro(cro_address);

    result = cro.VerifyHash(cro_size, crr_address);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error verifying CRO in CRR {:08X}", result.raw);
        process->vm_manager.UnmapRange(cro_address, cro_size);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    result = cro.Rebase(slot->loaded_crs, cro_size, data_segment_address, data_segment_size,
                        bss_segment_address, bss_segment_size, false);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error rebasing CRO {:08X}", result.raw);
        process->vm_manager.UnmapRange(cro_address, cro_size);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    result = cro.Link(slot->loaded_crs, link_on_load_bug_fix);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error linking CRO {:08X}", result.raw);
        process->vm_manager.UnmapRange(cro_address, cro_size);
        rb.Push(result);
        rb.Push<u32>(0);
        return;
    }

    cro.Register(slot->loaded_crs, auto_link);

    u32 fix_size = cro.Fix(fix_level);

    slot->memory_synchronizer.SynchronizeOriginalMemory(*process);

    // TODO(wwylele): verify the behaviour when buffer_ptr == address
    if (cro_buffer_ptr != cro_address) {
        if (fix_size != cro_size) {
            result = process->vm_manager.UnmapRange(cro_address + fix_size, cro_size - fix_size);
            if (result.IsError()) {
                LOG_ERROR(Service_LDR, "Error unmapping memory block {:08X}", result.raw);
                process->vm_manager.UnmapRange(cro_address, cro_size);
                rb.Push(result);
                rb.Push<u32>(0);
                return;
            }
        }

        // Changes the block size
        slot->memory_synchronizer.ResizeMemoryBlock(cro_address, cro_buffer_ptr, fix_size);
    }

    VAddr exe_begin;
    u32 exe_size;
    std::tie(exe_begin, exe_size) = cro.GetExecutablePages();
    if (exe_begin) {
        result = process->vm_manager.ReprotectRange(exe_begin, exe_size,
                                                    Kernel::VMAPermission::ReadExecute);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error reprotecting memory block {:08X}", result.raw);
            process->vm_manager.UnmapRange(cro_address, fix_size);
            rb.Push(result);
            rb.Push<u32>(0);
            return;
        }
    }

    Core::CPU().InvalidateCacheRange(cro_address, cro_size);

    LOG_INFO(Service_LDR, "CRO \"{}\" loaded at 0x{:08X}, fixed_end=0x{:08X}", cro.ModuleName(),
             cro_address, cro_address + fix_size);

    rb.Push(RESULT_SUCCESS, fix_size);
}

void RO::UnloadCRO(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 3, 2);
    VAddr cro_address = rp.Pop<u32>();
    u32 zero = rp.Pop<u32>();
    VAddr cro_buffer_ptr = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, cro_address=0x{:08X}, zero={}, cro_buffer_ptr=0x{:08X}",
              cro_address, zero, cro_buffer_ptr);

    CROHelper cro(cro_address);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
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

    LOG_INFO(Service_LDR, "Unloading CRO \"{}\"", cro.ModuleName());

    u32 fixed_size = cro.GetFixedSize();

    cro.Unregister(slot->loaded_crs);

    ResultCode result = cro.Unlink(slot->loaded_crs);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unlinking CRO {:08X}", result.raw);
        rb.Push(result);
        return;
    }

    // If the module is not fixed, clears all external/internal relocations
    // to restore the state before loading, so that it can be loaded again(?)
    if (!cro.IsFixed()) {
        result = cro.ClearRelocations();
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error clearing relocations {:08X}", result.raw);
            rb.Push(result);
            return;
        }
    }

    cro.Unrebase(false);

    slot->memory_synchronizer.SynchronizeOriginalMemory(*process);

    // TODO(wwylele): verify the behaviour when buffer_ptr == address
    if (cro_address != cro_buffer_ptr) {
        result = process->vm_manager.UnmapRange(cro_address, fixed_size);
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error unmapping CRO {:08X}", result.raw);
        }
        slot->memory_synchronizer.RemoveMemoryBlock(cro_address, cro_buffer_ptr);
    }

    Core::CPU().InvalidateCacheRange(cro_address, fixed_size);

    rb.Push(result);
}

void RO::LinkCRO(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 1, 2);
    VAddr cro_address = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, cro_address=0x{:08X}", cro_address);

    CROHelper cro(cro_address);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
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

    LOG_INFO(Service_LDR, "Linking CRO \"{}\"", cro.ModuleName());

    ResultCode result = cro.Link(slot->loaded_crs, false);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error linking CRO {:08X}", result.raw);
    }

    slot->memory_synchronizer.SynchronizeOriginalMemory(*process);

    rb.Push(result);
}

void RO::UnlinkCRO(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 1, 2);
    VAddr cro_address = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, cro_address=0x{:08X}", cro_address);

    CROHelper cro(cro_address);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
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

    LOG_INFO(Service_LDR, "Unlinking CRO \"{}\"", cro.ModuleName());

    ResultCode result = cro.Unlink(slot->loaded_crs);
    if (result.IsError()) {
        LOG_ERROR(Service_LDR, "Error unlinking CRO {:08X}", result.raw);
    }

    slot->memory_synchronizer.SynchronizeOriginalMemory(*process);

    rb.Push(result);
}

void RO::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 1, 2);
    VAddr crs_buffer_ptr = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    LOG_DEBUG(Service_LDR, "called, crs_buffer_ptr=0x{:08X}", crs_buffer_ptr);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    ClientSlot* slot = GetSessionData(ctx.Session());
    if (slot->loaded_crs == 0) {
        LOG_ERROR(Service_LDR, "Not initialized");
        rb.Push(ERROR_NOT_INITIALIZED);
        return;
    }

    CROHelper crs(slot->loaded_crs);
    crs.Unrebase(true);

    slot->memory_synchronizer.SynchronizeOriginalMemory(*process);

    ResultCode result = RESULT_SUCCESS;

    // TODO(wwylele): verify the behaviour when buffer_ptr == address
    if (slot->loaded_crs != crs_buffer_ptr) {
        result = process->vm_manager.UnmapRange(slot->loaded_crs, crs.GetFileSize());
        if (result.IsError()) {
            LOG_ERROR(Service_LDR, "Error unmapping CRS {:08X}", result.raw);
        }
        slot->memory_synchronizer.RemoveMemoryBlock(slot->loaded_crs, crs_buffer_ptr);
    }

    slot->loaded_crs = 0;
    rb.Push(result);
}

RO::RO() : ServiceFramework("ldr:ro", 2) {
    static const FunctionInfo functions[] = {
        {0x000100C2, &RO::Initialize, "Initialize"},
        {0x00020082, &RO::LoadCRR, "LoadCRR"},
        {0x00030042, &RO::UnloadCRR, "UnloadCRR"},
        {0x000402C2, &RO::LoadCRO<false>, "LoadCRO"},
        {0x000500C2, &RO::UnloadCRO, "UnloadCRO"},
        {0x00060042, &RO::LinkCRO, "LinkCRO"},
        {0x00070042, &RO::UnlinkCRO, "UnlinkCRO"},
        {0x00080042, &RO::Shutdown, "Shutdown"},
        {0x000902C2, &RO::LoadCRO<true>, "LoadCRO_New"},
    };
    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<RO>()->InstallAsService(service_manager);
}

} // namespace Service::LDR
