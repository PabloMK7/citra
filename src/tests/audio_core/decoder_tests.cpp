// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#if defined(HAVE_MF) || defined(HAVE_FFMPEG)

#include <catch2/catch.hpp>
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/shared_page.h"
#include "core/memory.h"

#include "audio_core/hle/decoder.h"
#ifdef HAVE_MF
#include "audio_core/hle/wmf_decoder.h"
#elif HAVE_FFMPEG
#include "audio_core/hle/ffmpeg_decoder.h"
#endif
#include "audio_fixures.h"

TEST_CASE("DSP HLE Audio Decoder", "[audio_core]") {
    // HACK: see comments of member timing
    Core::System::GetInstance().timing = std::make_unique<Core::Timing>();
    Core::System::GetInstance().memory = std::make_unique<Memory::MemorySystem>();
    Kernel::KernelSystem kernel(*Core::System::GetInstance().memory, 0);
    SECTION("decoder should produce correct samples") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        auto decoder =
#ifdef HAVE_MF
            std::make_unique<AudioCore::HLE::WMFDecoder>(*Core::System::GetInstance().memory);
#elif HAVE_FFMPEG
            std::make_unique<AudioCore::HLE::FFMPEGDecoder>(*Core::System::GetInstance().memory);
#endif
        AudioCore::HLE::BinaryRequest request;

        request.codec = AudioCore::HLE::DecoderCodec::AAC;
        request.cmd = AudioCore::HLE::DecoderCommand::Init;
        // initialize decoder
        std::optional<AudioCore::HLE::BinaryResponse> response = decoder->ProcessRequest(request);

        request.cmd = AudioCore::HLE::DecoderCommand::Decode;
        u8* fcram = Core::System::GetInstance().memory->GetFCRAMPointer(0);

        memcpy(fcram, fixure_buffer, fixure_buffer_size);
        request.src_addr = Memory::FCRAM_PADDR;
        request.dst_addr_ch0 = Memory::FCRAM_PADDR + 1024;
        request.dst_addr_ch1 = Memory::FCRAM_PADDR + 1048576; // 1 MB
        request.size = fixure_buffer_size;

        response = decoder->ProcessRequest(request);
        response = decoder->ProcessRequest(request);
        // remove this line
        request.src_addr = Memory::FCRAM_PADDR;
    }
}

#endif
