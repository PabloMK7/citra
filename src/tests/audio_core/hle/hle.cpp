// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "audio_core/hle/decoder.h"
#include "audio_core/hle/hle.h"
#include "audio_core/lle/lle.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/memory.h"

TEST_CASE("DSP LLE vs HLE", "[audio_core][hle]") {
    Core::System system;
    Memory::MemorySystem hle_memory{system};
    Core::Timing hle_core_timing(1, 100);

    Memory::MemorySystem lle_memory{system};
    Core::Timing lle_core_timing(1, 100);

    AudioCore::DspHle hle(hle_memory, hle_core_timing);
    AudioCore::DspLle lle(lle_memory, lle_core_timing, true);

    // Initialiase LLE
    {
        FileUtil::SetUserPath();
        // see tests/audio_core/lle/lle.cpp for details on dspaudio.cdc
        std::string firm_filepath =
            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) + "3ds" DIR_SEP "dspaudio.cdc";

        if (!FileUtil::Exists(firm_filepath)) {
            SKIP("Test requires dspaudio.cdc");
        }

        FileUtil::IOFile firm_file(firm_filepath, "rb");

        std::vector<u8> firm_file_buf(firm_file.GetSize());
        firm_file.ReadArray(firm_file_buf.data(), firm_file_buf.size());
        lle.LoadComponent(firm_file_buf);
        lle.SetSemaphoreHandler([&lle]() {
            u16 slot = lle.RecvData(2);
            u16 side = slot % 2;
            u16 pipe = slot / 2;
            fmt::print("SetSemaphoreHandler slot={}\n", slot);
            if (pipe > 15)
                return;
            if (side != 0)
                return;
            if (pipe == 0) {
                // pipe 0 is for debug. 3DS automatically drains this pipe and discards the
                // data
                lle.PipeRead(static_cast<AudioCore::DspPipe>(pipe),
                             lle.GetPipeReadableSize(static_cast<AudioCore::DspPipe>(pipe)));
            }
        });
        lle.SetRecvDataHandler(0, []() { fmt::print("SetRecvDataHandler 0\n"); });
        lle.SetRecvDataHandler(1, []() { fmt::print("SetRecvDataHandler 1\n"); });
        lle.SetRecvDataHandler(2, []() { fmt::print("SetRecvDataHandler 2\n"); });
    }

    SECTION("Initialise Audio Pipe") {
        std::vector<u8> buffer(4, 0);
        buffer[0] = 0;

        // LLE
        {
            lle.PipeWrite(AudioCore::DspPipe::Audio, buffer);
            lle.SetSemaphore(0x4000);

            // todo: wait for interrupt
            do {
                lle_core_timing.GetTimer(0)->AddTicks(lle_core_timing.GetTimer(0)->GetDowncount());
                lle_core_timing.GetTimer(0)->Advance();
                lle_core_timing.GetTimer(0)->SetNextSlice();
            } while (lle.GetPipeReadableSize(AudioCore::DspPipe::Audio) == 0);

            REQUIRE(lle.GetPipeReadableSize(AudioCore::DspPipe::Audio) >= 32);
        }
        std::vector<u8> lle_read_buffer;
        lle_read_buffer = lle.PipeRead(AudioCore::DspPipe::Audio, 2);
        u16 lle_size;
        std::memcpy(&lle_size, lle_read_buffer.data(), sizeof(lle_size));
        lle_read_buffer = lle.PipeRead(AudioCore::DspPipe::Audio, lle_size * 2);

        // HLE
        {
            hle.PipeWrite(AudioCore::DspPipe::Audio, buffer);
            REQUIRE(hle.GetPipeReadableSize(AudioCore::DspPipe::Audio) >= 32);
        }
        std::vector<u8> hle_read_buffer(32);
        hle_read_buffer = hle.PipeRead(AudioCore::DspPipe::Audio, 2);
        u16 hle_size;
        std::memcpy(&hle_size, hle_read_buffer.data(), sizeof(hle_size));
        hle_read_buffer = hle.PipeRead(AudioCore::DspPipe::Audio, hle_size * 2);

        REQUIRE(hle_size == lle_size);
        REQUIRE(hle_read_buffer == lle_read_buffer);
    }

    SECTION("Initialise Binary Pipe") {
        std::vector<u8> buffer(32, 0);
        AudioCore::HLE::BinaryMessage& request =
            *reinterpret_cast<AudioCore::HLE::BinaryMessage*>(buffer.data());

        request.header.codec = AudioCore::HLE::DecoderCodec::DecodeAAC;
        request.header.cmd = AudioCore::HLE::DecoderCommand::Init;

        // Values used by Pokemon X
        request.header.result = static_cast<AudioCore::HLE::ResultStatus>(3);
        request.decode_aac_init.unknown1 = 1;
        request.decode_aac_init.unknown2 = 0xFFFF'FFFF;
        request.decode_aac_init.unknown3 = 1;
        request.decode_aac_init.unknown4 = 0;
        request.decode_aac_init.unknown5 = 1;
        request.decode_aac_init.unknown6 = 0x20;

        // LLE
        lle.PipeWrite(AudioCore::DspPipe::Binary, buffer);
        lle.SetSemaphore(0x4000);

        // todo: wait for interrupt
        do {
            lle_core_timing.GetTimer(0)->AddTicks(lle_core_timing.GetTimer(0)->GetDowncount());
            lle_core_timing.GetTimer(0)->Advance();
            lle_core_timing.GetTimer(0)->SetNextSlice();
        } while (lle.GetPipeReadableSize(AudioCore::DspPipe::Binary) == 0);

        REQUIRE(lle.GetPipeReadableSize(AudioCore::DspPipe::Binary) >= 32);

        std::vector<u8> lle_read_buffer = lle.PipeRead(AudioCore::DspPipe::Binary, 32);
        AudioCore::HLE::BinaryMessage& resp =
            *reinterpret_cast<AudioCore::HLE::BinaryMessage*>(lle_read_buffer.data());
        CHECK(resp.header.result == AudioCore::HLE::ResultStatus::Success);

        // HLE
        {
            hle.PipeWrite(AudioCore::DspPipe::Binary, buffer);
            REQUIRE(hle.GetPipeReadableSize(AudioCore::DspPipe::Binary) >= 32);
        }
        std::vector<u8> hle_read_buffer = hle.PipeRead(AudioCore::DspPipe::Binary, 32);

        REQUIRE(hle_read_buffer == lle_read_buffer);
    }
}
