// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/lle/lle.h"
#include "common/assert.h"
#include "common/swap.h"
#include "teakra/teakra.h"

namespace AudioCore {

struct PipeStatus {
    u16_le waddress;
    u16_le bsize;
    u16_le read_bptr;
    u16_le write_bptr;
    u8 slot_index;
    u8 flags;
};

static_assert(sizeof(PipeStatus) == 10);

enum class PipeDirection : u8 {
    DSPtoCPU = 0,
    CPUtoDSP = 1,
};

static u8 PipeIndexToSlotIndex(u8 pipe_index, PipeDirection direction) {
    return (pipe_index << 1) + (u8)direction;
}

struct DspLle::Impl final {
    Teakra::Teakra teakra;
    u16 pipe_base_waddr = 0;

    static constexpr unsigned TeakraSlice = 20000;
    void RunTeakraSlice() {
        teakra.Run(TeakraSlice);
    }

    u8* GetDspDataPointer(u32 baddr) {
        auto& memory = teakra.GetDspMemory();
        return &memory[0x40000 + baddr];
    }

    PipeStatus GetPipeStatus(u8 pipe_index, PipeDirection direction) {
        u8 slot_index = PipeIndexToSlotIndex(pipe_index, direction);
        PipeStatus pipe_status;
        std::memcpy(&pipe_status,
                    GetDspDataPointer(pipe_base_waddr * 2 + slot_index * sizeof(PipeStatus)),
                    sizeof(PipeStatus));
        ASSERT(pipe_status.slot_index == slot_index);
        return pipe_status;
    }

    void UpdatePipeStatus(const PipeStatus& pipe_status) {
        u8 slot_index = pipe_status.slot_index;
        u8* status_address =
            GetDspDataPointer(pipe_base_waddr * 2 + slot_index * sizeof(PipeStatus));
        if (slot_index % 2 == 0) {
            std::memcpy(status_address + 4, &pipe_status.read_bptr, sizeof(u16));
        } else {
            std::memcpy(status_address + 6, &pipe_status.write_bptr, sizeof(u16));
        }
    }

    void WritePipe(u8 pipe_index, const std::vector<u8>& data) {
        PipeStatus pipe_status = GetPipeStatus(pipe_index, PipeDirection::CPUtoDSP);
        bool need_update = false;
        const u8* buffer_ptr = data.data();
        u16 bsize = (u16)data.size();
        while (bsize != 0) {
            u16 x = pipe_status.read_bptr ^ pipe_status.write_bptr;
            ASSERT_MSG(x != 0x8000, "Pipe is Full");
            u16 write_bend;
            if (x > 0x8000)
                write_bend = pipe_status.read_bptr & 0x7FFF;
            else
                write_bend = pipe_status.bsize;
            u16 write_bbegin = pipe_status.write_bptr & 0x7FFF;
            ASSERT_MSG(write_bend > write_bbegin,
                       "Pipe is in inconsistent state: end {:04X} <= begin {:04X}, size {:04X}",
                       write_bend, write_bbegin, pipe_status.bsize);
            u16 write_bsize = std::min<u16>(bsize, write_bend - write_bbegin);
            std::memcpy(GetDspDataPointer(pipe_status.waddress * 2 + write_bbegin), buffer_ptr,
                        write_bsize);
            buffer_ptr += write_bsize;
            pipe_status.write_bptr += write_bsize;
            bsize -= write_bsize;
            ASSERT_MSG((pipe_status.write_bptr & 0x7FFF) <= pipe_status.bsize,
                       "Pipe is in inconsistent state: write > size");
            if ((pipe_status.write_bptr & 0x7FFF) == pipe_status.bsize) {
                pipe_status.write_bptr &= 0x8000;
                pipe_status.write_bptr ^= 0x8000;
            }
            need_update = true;
        }
        if (need_update) {
            UpdatePipeStatus(pipe_status);
            while (!teakra.SendDataIsEmpty(2))
                RunTeakraSlice();
            teakra.SendData(2, pipe_status.slot_index);
        }
    }
};

u16 DspLle::RecvData(u32 register_number) {
    while (!impl->teakra.RecvDataIsReady(register_number)) {
        impl->RunTeakraSlice();
    }
    return impl->teakra.RecvData(static_cast<u8>(register_number));
}

bool DspLle::RecvDataIsReady(u32 register_number) const {
    return impl->teakra.RecvDataIsReady(register_number);
}

void DspLle::SetSemaphore(u16 semaphore_value) {
    impl->teakra.SetSemaphore(semaphore_value);
}

void DspLle::PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer) {
    impl->WritePipe(static_cast<u8>(pipe_number), buffer);
}

DspLle::DspLle() : impl(std::make_unique<Impl>()) {}
DspLle::~DspLle() = default;

} // namespace AudioCore
