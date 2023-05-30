// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "audio_core/hle/decoder.h"
#include "audio_core/lle/lle.h"
#include "common/common_paths.h"
#include "core/core_timing.h"
#include "core/memory.h"

TEST_CASE("DSP LLE Sanity", "[audio_core][lle]") {
    Memory::MemorySystem memory;
    Core::Timing core_timing(1, 100);

    AudioCore::DspLle lle(memory, core_timing, true);
    {
        FileUtil::SetUserPath();
        // dspaudio.cdc can be dumped from Pokemon X & Y, It can be found in the romfs at
        // "rom:/sound/dspaudio.cdc".
        // One could also extract the firmware from the 3DS sound app using a modified version of
        // https://github.com/zoogie/DSP1.
        std::string firm_filepath =
            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) + "3ds" DIR_SEP "dspaudio.cdc";

        if (!FileUtil::Exists(firm_filepath)) {
            SKIP("Test requires dspaudio.cdc");
        }

        FileUtil::IOFile firm_file(firm_filepath, "rb");

        std::vector<u8> firm_file_buf(firm_file.GetSize());
        firm_file.ReadArray(firm_file_buf.data(), firm_file_buf.size());
        lle.LoadComponent(firm_file_buf);
    }
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
    SECTION("Initialise Audio Pipe") {
        std::vector<u8> buffer(4, 0);
        buffer[0] = 0;

        lle.PipeWrite(AudioCore::DspPipe::Audio, buffer);
        lle.SetSemaphore(0x4000);

        // todo: wait for interrupt
        do {
            core_timing.GetTimer(0)->AddTicks(core_timing.GetTimer(0)->GetDowncount());
            core_timing.GetTimer(0)->Advance();
            core_timing.GetTimer(0)->SetNextSlice();
        } while (lle.GetPipeReadableSize(AudioCore::DspPipe::Audio) == 0);

        REQUIRE(lle.GetPipeReadableSize(AudioCore::DspPipe::Audio) >= 32);

        buffer = lle.PipeRead(AudioCore::DspPipe::Audio, 2);
        u16 size;
        memcpy(&size, buffer.data(), sizeof(size));
        // see AudioCore::DspHle::Impl::AudioPipeWriteStructAddresses()
        REQUIRE(size * 2 == 30);
    }
    SECTION("Initialise EncodeAAC - Binary Pipe") {
        std::vector<u8> buffer(32, 0);
        AudioCore::HLE::BinaryMessage& request =
            *reinterpret_cast<AudioCore::HLE::BinaryMessage*>(buffer.data());

        request.header.codec = AudioCore::HLE::DecoderCodec::EncodeAAC;
        request.header.cmd = AudioCore::HLE::DecoderCommand::Init;

        // Values used by the 3DS sound app.
        request.encode_aac_init.unknown1 = 1;
        request.encode_aac_init.sample_rate = static_cast<AudioCore::HLE::DecoderSampleRate>(5);
        request.encode_aac_init.unknown3 = 2;
        request.encode_aac_init.unknown4 = 0;
        request.encode_aac_init.unknown5 = rand();
        request.encode_aac_init.unknown6 = rand();
        lle.PipeWrite(AudioCore::DspPipe::Binary, buffer);
        lle.SetSemaphore(0x4000);

        // todo: wait for interrupt
        do {
            core_timing.GetTimer(0)->AddTicks(core_timing.GetTimer(0)->GetDowncount());
            core_timing.GetTimer(0)->Advance();
            core_timing.GetTimer(0)->SetNextSlice();
        } while (lle.GetPipeReadableSize(AudioCore::DspPipe::Binary) == 0);

        REQUIRE(lle.GetPipeReadableSize(AudioCore::DspPipe::Binary) >= 32);

        buffer = lle.PipeRead(AudioCore::DspPipe::Binary, 32);
        AudioCore::HLE::BinaryMessage& resp =
            *reinterpret_cast<AudioCore::HLE::BinaryMessage*>(buffer.data());
        REQUIRE(resp.header.result == AudioCore::HLE::ResultStatus::Success);
        REQUIRE(resp.data == request.data);
    }
}
