// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "audio_core/lle/lle.h"
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/dsp/dsp_dsp.h"
#include "teakra/teakra.h"

namespace AudioCore {

enum class SegmentType : u8 {
    ProgramA = 0,
    ProgramB = 1,
    Data = 2,
};

class Dsp1 {
public:
    Dsp1(const std::vector<u8>& raw);

    struct Header {
        std::array<u8, 0x100> signature;
        std::array<u8, 4> magic;
        u32_le binary_size;
        u16_le memory_layout;
        INSERT_PADDING_BYTES(3);
        SegmentType special_segment_type;
        u8 num_segments;
        union {
            BitField<0, 1, u8> recv_data_on_start;
            BitField<1, 1, u8> load_special_segment;
        };
        u32_le special_segment_address;
        u32_le special_segment_size;
        u64_le zero;
        struct Segment {
            u32_le offset;
            u32_le address;
            u32_le size;
            INSERT_PADDING_BYTES(3);
            SegmentType memory_type;
            std::array<u8, 0x20> sha256;
        };
        std::array<Segment, 10> segments;
    };
    static_assert(sizeof(Header) == 0x300);

    struct Segment {
        std::vector<u8> data;
        SegmentType memory_type;
        u32 target;
    };

    std::vector<Segment> segments;
    bool recv_data_on_start;
};

Dsp1::Dsp1(const std::vector<u8>& raw) {
    Header header;
    std::memcpy(&header, raw.data(), sizeof(header));
    recv_data_on_start = header.recv_data_on_start != 0;
    for (u32 i = 0; i < header.num_segments; ++i) {
        Segment segment;
        segment.data =
            std::vector<u8>(raw.begin() + header.segments[i].offset,
                            raw.begin() + header.segments[i].offset + header.segments[i].size);
        segment.memory_type = header.segments[i].memory_type;
        segment.target = header.segments[i].address;
        segments.push_back(segment);
    }
}

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
    Impl() {
        teakra_slice_event = Core::System::GetInstance().CoreTiming().RegisterEvent(
            "DSP slice", [this](u64, int late) { TeakraSliceEvent(static_cast<u64>(late)); });
    }

    Teakra::Teakra teakra;
    u16 pipe_base_waddr = 0;

    bool semaphore_signaled = false;
    bool data_signaled = false;

    Core::TimingEventType* teakra_slice_event;

    static constexpr unsigned TeakraSlice = 20000;
    void RunTeakraSlice() {
        teakra.Run(TeakraSlice);
    }

    void TeakraSliceEvent(u64 late) {
        RunTeakraSlice();
        u64 next = TeakraSlice * 2; // DSP runs at clock rate half of the CPU rate
        if (next < late)
            next = 0;
        else
            next -= late;
        Core::System::GetInstance().CoreTiming().ScheduleEvent(next, teakra_slice_event, 0);
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

    std::vector<u8> ReadPipe(u8 pipe_index, u16 bsize) {
        PipeStatus pipe_status = GetPipeStatus(pipe_index, PipeDirection::DSPtoCPU);
        bool need_update = false;
        std::vector<u8> data(bsize);
        u8* buffer_ptr = data.data();
        while (bsize != 0) {
            u16 x = pipe_status.read_bptr ^ pipe_status.write_bptr;
            ASSERT_MSG(x != 0, "Pipe is empty");
            u16 read_bend;
            if (x >= 0x8000) {
                read_bend = pipe_status.bsize;
            } else {
                read_bend = pipe_status.write_bptr & 0x7FFF;
            }
            u16 read_bbegin = pipe_status.read_bptr & 0x7FFF;
            ASSERT(read_bend > read_bbegin);
            u16 read_bsize = std::min<u16>(bsize, read_bend - read_bbegin);
            std::memcpy(buffer_ptr, GetDspDataPointer(pipe_status.waddress * 2 + read_bbegin),
                        read_bsize);
            buffer_ptr += read_bsize;
            pipe_status.read_bptr += read_bsize;
            bsize -= read_bsize;
            ASSERT_MSG((pipe_status.read_bptr & 0x7FFF) <= pipe_status.bsize,
                       "Pipe is in inconsistent state: read > size");
            if ((pipe_status.read_bptr & 0x7FFF) == pipe_status.bsize) {
                pipe_status.read_bptr &= 0x8000;
                pipe_status.read_bptr ^= 0x8000;
            }
            need_update = true;
        }
        if (need_update) {
            UpdatePipeStatus(pipe_status);
            while (!teakra.SendDataIsEmpty(2))
                RunTeakraSlice();
            teakra.SendData(2, pipe_status.slot_index);
        }
        return data;
    }
    u16 GetPipeReadableSize(u8 pipe_index) {
        PipeStatus pipe_status = GetPipeStatus(pipe_index, PipeDirection::DSPtoCPU);
        u16 size = pipe_status.write_bptr - pipe_status.read_bptr;
        if ((pipe_status.read_bptr ^ pipe_status.write_bptr) >= 0x8000) {
            size += pipe_status.bsize;
        }
        return size & 0x7FFF;
    }

    void LoadComponent(const std::vector<u8>& buffer) {
        Dsp1 dsp(buffer);
        auto& dsp_memory = teakra.GetDspMemory();
        u8* program = dsp_memory.data();
        u8* data = dsp_memory.data() + 0x40000;
        dsp_memory.fill(0);
        for (const auto& segment : dsp.segments) {
            if (segment.memory_type == SegmentType::ProgramA ||
                segment.memory_type == SegmentType::ProgramB) {
                std::memcpy(program + segment.target * 2, segment.data.data(), segment.data.size());
            } else if (segment.memory_type == SegmentType::Data) {
                std::memcpy(data + segment.target * 2, segment.data.data(), segment.data.size());
            }
        }

        // TODO: load special segment

        Core::System::GetInstance().CoreTiming().ScheduleEvent(TeakraSlice, teakra_slice_event, 0);

        // Wait for initialization
        if (dsp.recv_data_on_start) {
            for (unsigned i = 0; i < 3; ++i) {
                while (!teakra.RecvDataIsReady(i))
                    RunTeakraSlice();
                ASSERT(teakra.RecvData(i) == 1);
            }
        }

        // Get pipe base address
        while (!teakra.RecvDataIsReady(2))
            RunTeakraSlice();
        pipe_base_waddr = teakra.RecvData(2);
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

std::vector<u8> DspLle::PipeRead(DspPipe pipe_number, u32 length) {
    return impl->ReadPipe(static_cast<u8>(pipe_number), static_cast<u16>(length));
}

std::size_t DspLle::GetPipeReadableSize(DspPipe pipe_number) const {
    return impl->GetPipeReadableSize(static_cast<u8>(pipe_number));
}

void DspLle::PipeWrite(DspPipe pipe_number, const std::vector<u8>& buffer) {
    impl->WritePipe(static_cast<u8>(pipe_number), buffer);
}

std::array<u8, Memory::DSP_RAM_SIZE>& DspLle::GetDspMemory() {
    return impl->teakra.GetDspMemory();
}

void DspLle::SetServiceToInterrupt(std::weak_ptr<Service::DSP::DSP_DSP> dsp) {
    impl->teakra.SetRecvDataHandler(0, [dsp]() {
        if (auto locked = dsp.lock()) {
            locked->SignalInterrupt(Service::DSP::DSP_DSP::InterruptType::Zero,
                                    static_cast<DspPipe>(0));
        }
    });
    impl->teakra.SetRecvDataHandler(1, [dsp]() {
        if (auto locked = dsp.lock()) {
            locked->SignalInterrupt(Service::DSP::DSP_DSP::InterruptType::One,
                                    static_cast<DspPipe>(0));
        }
    });

    auto ProcessPipeEvent = [this, dsp](bool event_from_data) {
        auto& teakra = impl->teakra;
        if (event_from_data) {
            impl->data_signaled = true;
        } else {
            if ((teakra.GetSemaphore() & 0x8000) == 0)
                return;
            impl->semaphore_signaled = true;
        }
        if (impl->semaphore_signaled && impl->data_signaled) {
            impl->semaphore_signaled = impl->data_signaled = false;
            u16 slot = teakra.RecvData(2);
            u16 side = slot % 2;
            u16 pipe = slot / 2;
            ASSERT(pipe < 16);
            if (side != static_cast<u16>(PipeDirection::DSPtoCPU))
                return;
            if (pipe == 0) {
                // pipe 0 is for debug. 3DS automatically drains this pipe and discards the data
                impl->ReadPipe(pipe, impl->GetPipeReadableSize(pipe));
            } else {
                if (auto locked = dsp.lock()) {
                    locked->SignalInterrupt(Service::DSP::DSP_DSP::InterruptType::Pipe,
                                            static_cast<DspPipe>(pipe));
                }
            }
        }
    };

    impl->teakra.SetRecvDataHandler(2, [ProcessPipeEvent]() { ProcessPipeEvent(true); });
    impl->teakra.SetSemaphoreHandler([ProcessPipeEvent]() { ProcessPipeEvent(false); });
}

void DspLle::LoadComponent(const std::vector<u8>& buffer) {
    impl->LoadComponent(buffer);
}

DspLle::DspLle() : impl(std::make_unique<Impl>()) {}
DspLle::~DspLle() = default;

} // namespace AudioCore
