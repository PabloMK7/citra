// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fdk-aac/aacdecoder_lib.h>
#include "audio_core/hle/fdk_decoder.h"

namespace AudioCore::HLE {

class FDKDecoder::Impl {
public:
    explicit Impl(Memory::MemorySystem& memory);
    ~Impl();
    std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request);
    bool IsValid() const {
        return decoder != nullptr;
    }

private:
    std::optional<BinaryResponse> Initalize(const BinaryRequest& request);

    std::optional<BinaryResponse> Decode(const BinaryRequest& request);

    void Clear();

    Memory::MemorySystem& memory;

    HANDLE_AACDECODER decoder = nullptr;
};

FDKDecoder::Impl::Impl(Memory::MemorySystem& memory) : memory(memory) {
    // allocate an array of LIB_INFO structures
    // if we don't pre-fill the whole segment with zeros, when we call `aacDecoder_GetLibInfo`
    // it will segfault, upon investigation, there is some code in fdk_aac depends on your initial
    // values in this array
    LIB_INFO decoder_info[FDK_MODULE_LAST] = {};
    // get library information and fill the struct
    if (aacDecoder_GetLibInfo(decoder_info) != 0) {
        LOG_ERROR(Audio_DSP, "Failed to retrieve fdk_aac library information!");
        return;
    }
    // This segment: identify the broken fdk_aac implementation
    // and refuse to initialize if identified as broken (check for module IDs)
    // although our AAC samples do not contain SBC feature, this is a way to detect
    // watered down version of fdk_aac implementations
    if (FDKlibInfo_getCapabilities(decoder_info, FDK_SBRDEC) == 0) {
        LOG_ERROR(Audio_DSP, "Bad fdk_aac library found! Initialization aborted!");
        return;
    }

    LOG_INFO(Audio_DSP, "Using fdk_aac version {} (build date: {})", decoder_info[0].versionStr,
             decoder_info[0].build_date);

    // choose the input format when initializing: 1 layer of ADTS
    decoder = aacDecoder_Open(TRANSPORT_TYPE::TT_MP4_ADTS, 1);
    // set maximum output channel to two (stereo)
    // if the input samples have more channels, fdk_aac will perform a downmix
    AAC_DECODER_ERROR ret = aacDecoder_SetParam(decoder, AAC_PCM_MAX_OUTPUT_CHANNELS, 2);
    if (ret != AAC_DEC_OK) {
        // unable to set this parameter reflects the decoder implementation might be broken
        // we'd better shuts down everything
        aacDecoder_Close(decoder);
        decoder = nullptr;
        LOG_ERROR(Audio_DSP, "Unable to set downmix parameter: {}", ret);
        return;
    }
}

std::optional<BinaryResponse> FDKDecoder::Impl::Initalize(const BinaryRequest& request) {
    BinaryResponse response;
    std::memcpy(&response, &request, sizeof(response));
    response.unknown1 = 0x0;

    if (decoder) {
        LOG_INFO(Audio_DSP, "FDK Decoder initialized");
        Clear();
    } else {
        LOG_ERROR(Audio_DSP, "Decoder not initialized");
    }

    return response;
}

FDKDecoder::Impl::~Impl() {
    if (decoder)
        aacDecoder_Close(decoder);
}

void FDKDecoder::Impl::Clear() {
    s16 decoder_output[8192];
    // flush and re-sync the decoder, discarding the internal buffer
    // we actually don't care if this succeeds or not
    // FLUSH - flush internal buffer
    // INTR - treat the current internal buffer as discontinuous
    // CONCEAL - try to interpolate and smooth out the samples
    if (decoder)
        aacDecoder_DecodeFrame(decoder, decoder_output, 8192,
                               AACDEC_FLUSH & AACDEC_INTR & AACDEC_CONCEAL);
}

std::optional<BinaryResponse> FDKDecoder::Impl::ProcessRequest(const BinaryRequest& request) {
    if (request.codec != DecoderCodec::AAC) {
        LOG_ERROR(Audio_DSP, "FDK AAC Decoder cannot handle such codec: {}",
                  static_cast<u16>(request.codec));
        return {};
    }

    switch (request.cmd) {
    case DecoderCommand::Init: {
        return Initalize(request);
    }
    case DecoderCommand::Decode: {
        return Decode(request);
    }
    case DecoderCommand::Unknown: {
        BinaryResponse response;
        std::memcpy(&response, &request, sizeof(response));
        response.unknown1 = 0x0;
        return response;
    }
    default:
        LOG_ERROR(Audio_DSP, "Got unknown binary request: {}", static_cast<u16>(request.cmd));
        return {};
    }
}

std::optional<BinaryResponse> FDKDecoder::Impl::Decode(const BinaryRequest& request) {
    BinaryResponse response;
    response.codec = request.codec;
    response.cmd = request.cmd;
    response.size = request.size;

    if (!decoder) {
        LOG_DEBUG(Audio_DSP, "Decoder not initalized");
        // This is a hack to continue games that are not compiled with the aac codec
        response.num_channels = 2;
        response.num_samples = 1024;
        return response;
    }

    if (request.src_addr < Memory::FCRAM_PADDR ||
        request.src_addr + request.size > Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
        LOG_ERROR(Audio_DSP, "Got out of bounds src_addr {:08x}", request.src_addr);
        return {};
    }
    u8* data = memory.GetFCRAMPointer(request.src_addr - Memory::FCRAM_PADDR);

    std::array<std::vector<s16>, 2> out_streams;

    std::size_t data_size = request.size;

    // decoding loops
    AAC_DECODER_ERROR result = AAC_DEC_OK;
    // 8192 units of s16 are enough to hold one frame of AAC-LC or AAC-HE/v2 data
    s16 decoder_output[8192];
    // note that we don't free this pointer as it is automatically freed by fdk_aac
    CStreamInfo* stream_info;
    // how many bytes to be queued into the decoder, decrementing from the buffer size
    u32 buffer_remaining = data_size;
    // alias the data_size as an u32
    u32 input_size = data_size;

    while (buffer_remaining) {
        // queue the input buffer, fdk_aac will automatically slice out the buffer it needs
        // from the input buffer
        result = aacDecoder_Fill(decoder, &data, &input_size, &buffer_remaining);
        if (result != AAC_DEC_OK) {
            // there are some issues when queuing the input buffer
            LOG_ERROR(Audio_DSP, "Failed to enqueue the input samples");
            return std::nullopt;
        }
        // get output from decoder
        result = aacDecoder_DecodeFrame(decoder, decoder_output, 8192, 0);
        if (result == AAC_DEC_OK) {
            // get the stream information
            stream_info = aacDecoder_GetStreamInfo(decoder);
            // fill the stream information for binary response
            response.sample_rate = GetSampleRateEnum(stream_info->sampleRate);
            response.num_channels = stream_info->aacNumChannels;
            response.num_samples = stream_info->frameSize;
            // fill the output
            // the sample size = frame_size * channel_counts
            for (int sample = 0; sample < (stream_info->frameSize * 2); sample++) {
                for (int ch = 0; ch < stream_info->aacNumChannels; ch++) {
                    out_streams[ch].push_back(decoder_output[(sample * 2) + 1]);
                }
            }
        } else if (result == AAC_DEC_TRANSPORT_SYNC_ERROR) {
            // decoder has some synchronization problems, try again with new samples,
            // using old samples might trigger this error again
            continue;
        } else {
            LOG_ERROR(Audio_DSP, "Error decoding the sample: {}", result);
            return std::nullopt;
        }
    }
    // transfer the decoded buffer from vector to the FCRAM
    if (out_streams[0].size() != 0) {
        if (request.dst_addr_ch0 < Memory::FCRAM_PADDR ||
            request.dst_addr_ch0 + out_streams[0].size() >
                Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
            LOG_ERROR(Audio_DSP, "Got out of bounds dst_addr_ch0 {:08x}", request.dst_addr_ch0);
            return {};
        }
        std::memcpy(memory.GetFCRAMPointer(request.dst_addr_ch0 - Memory::FCRAM_PADDR),
                    out_streams[0].data(), out_streams[0].size());
    }

    if (out_streams[1].size() != 0) {
        if (request.dst_addr_ch1 < Memory::FCRAM_PADDR ||
            request.dst_addr_ch1 + out_streams[1].size() >
                Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
            LOG_ERROR(Audio_DSP, "Got out of bounds dst_addr_ch1 {:08x}", request.dst_addr_ch1);
            return {};
        }
        std::memcpy(memory.GetFCRAMPointer(request.dst_addr_ch1 - Memory::FCRAM_PADDR),
                    out_streams[1].data(), out_streams[1].size());
    }
    return response;
}

FDKDecoder::FDKDecoder(Memory::MemorySystem& memory) : impl(std::make_unique<Impl>(memory)) {}

FDKDecoder::~FDKDecoder() = default;

std::optional<BinaryResponse> FDKDecoder::ProcessRequest(const BinaryRequest& request) {
    return impl->ProcessRequest(request);
}

bool FDKDecoder::IsValid() const {
    return impl->IsValid();
}

} // namespace AudioCore::HLE
