// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaError.h>
#include <media/NdkMediaFormat.h>

#include <memory>
#include <vector>

#include "audio_core/hle/adts.h"
#include "audio_core/hle/mediandk_decoder.h"

namespace AudioCore::HLE {

struct AMediaCodecRelease {
    void operator()(AMediaCodec* codec) const {
        AMediaCodec_stop(codec);
        AMediaCodec_delete(codec);
    };
};

class MediaNDKDecoder::Impl {
public:
    explicit Impl(Memory::MemorySystem& memory);
    ~Impl();
    std::optional<BinaryMessage> ProcessRequest(const BinaryMessage& request);

    bool SetMediaType(const AudioCore::ADTSData& adts_data);

private:
    std::optional<BinaryMessage> Initalize(const BinaryMessage& request);
    std::optional<BinaryMessage> Decode(const BinaryMessage& request);

    Memory::MemorySystem& memory;
    std::unique_ptr<AMediaCodec, AMediaCodecRelease> decoder;
    // default: 2 channles, 48000 samplerate
    AudioCore::ADTSData mADTSData{
        /*header_length*/ 7,  /*mpeg2*/ false,   /*profile*/ 2,
        /*channels*/ 2,       /*channel_idx*/ 2, /*framecount*/ 0,
        /*samplerate_idx*/ 3, /*length*/ 0,      /*samplerate*/ 48000};
};

MediaNDKDecoder::Impl::Impl(Memory::MemorySystem& memory_) : memory(memory_) {
    SetMediaType(mADTSData);
}

MediaNDKDecoder::Impl::~Impl() = default;

std::optional<BinaryMessage> MediaNDKDecoder::Impl::Initalize(const BinaryMessage& request) {
    BinaryMessage response = request;
    response.header.result = ResultStatus::Success;
    return response;
}

bool MediaNDKDecoder::Impl::SetMediaType(const AudioCore::ADTSData& adts_data) {
    const char* mime = "audio/mp4a-latm";
    if (decoder && mADTSData.profile == adts_data.profile &&
        mADTSData.channel_idx == adts_data.channel_idx &&
        mADTSData.samplerate_idx == adts_data.samplerate_idx) {
        return true;
    }
    decoder.reset(AMediaCodec_createDecoderByType(mime));
    if (decoder == nullptr) {
        return false;
    }

    u8 csd_0[2];
    csd_0[0] = static_cast<u8>((adts_data.profile << 3) | (adts_data.samplerate_idx >> 1));
    csd_0[1] =
        static_cast<u8>(((adts_data.samplerate_idx << 7) & 0x80) | (adts_data.channel_idx << 3));
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, adts_data.samplerate);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, adts_data.channels);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_IS_ADTS, 1);
    AMediaFormat_setBuffer(format, "csd-0", csd_0, sizeof(csd_0));

    media_status_t status = AMediaCodec_configure(decoder.get(), format, NULL, NULL, 0);
    if (status != AMEDIA_OK) {
        AMediaFormat_delete(format);
        decoder.reset();
        return false;
    }

    status = AMediaCodec_start(decoder.get());
    if (status != AMEDIA_OK) {
        AMediaFormat_delete(format);
        decoder.reset();
        return false;
    }

    AMediaFormat_delete(format);
    mADTSData = adts_data;
    return true;
}

std::optional<BinaryMessage> MediaNDKDecoder::Impl::ProcessRequest(const BinaryMessage& request) {
    if (request.header.codec != DecoderCodec::DecodeAAC) {
        LOG_ERROR(Audio_DSP, "AAC Decoder cannot handle such codec: {}",
                  static_cast<u16>(request.header.codec));
        return {};
    }

    switch (request.header.cmd) {
    case DecoderCommand::Init: {
        return Initalize(request);
    }
    case DecoderCommand::EncodeDecode: {
        return Decode(request);
    }
    case DecoderCommand::Shutdown:
    case DecoderCommand::SaveState:
    case DecoderCommand::LoadState: {
        LOG_WARNING(Audio_DSP, "Got unimplemented binary request: {}",
                    static_cast<u16>(request.header.cmd));
        BinaryMessage response = request;
        response.header.result = ResultStatus::Success;
        return response;
    }
    default:
        LOG_ERROR(Audio_DSP, "Got unknown binary request: {}",
                  static_cast<u16>(request.header.cmd));
        return {};
    }
}

std::optional<BinaryMessage> MediaNDKDecoder::Impl::Decode(const BinaryMessage& request) {
    BinaryMessage response{};
    response.header.codec = request.header.codec;
    response.header.cmd = request.header.cmd;
    response.decode_aac_response.size = request.decode_aac_request.size;
    response.decode_aac_response.num_samples = 1024;

    if (request.decode_aac_request.src_addr < Memory::FCRAM_PADDR ||
        request.decode_aac_request.src_addr + request.decode_aac_request.size >
            Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
        LOG_ERROR(Audio_DSP, "Got out of bounds src_addr {:08x}",
                  request.decode_aac_request.src_addr);
        return response;
    }

    const u8* data =
        memory.GetFCRAMPointer(request.decode_aac_request.src_addr - Memory::FCRAM_PADDR);
    ADTSData adts_data = AudioCore::ParseADTS(data);
    SetMediaType(adts_data);
    response.decode_aac_response.sample_rate = GetSampleRateEnum(adts_data.samplerate);
    response.decode_aac_response.num_channels = adts_data.channels;
    if (!decoder) {
        LOG_ERROR(Audio_DSP, "Missing decoder for profile: {}, channels: {}, samplerate: {}",
                  adts_data.profile, adts_data.channels, adts_data.samplerate);
        return {};
    }

    // input
    constexpr int timeout = 160;
    std::size_t buffer_size = 0;
    u8* buffer = nullptr;
    ssize_t buffer_index = AMediaCodec_dequeueInputBuffer(decoder.get(), timeout);
    if (buffer_index < 0) {
        LOG_ERROR(Audio_DSP, "Failed to enqueue the input samples: {}", buffer_index);
        return response;
    }
    buffer = AMediaCodec_getInputBuffer(decoder.get(), buffer_index, &buffer_size);
    if (buffer_size < request.decode_aac_request.size) {
        return response;
    }
    std::memcpy(buffer, data, request.decode_aac_request.size);
    media_status_t status = AMediaCodec_queueInputBuffer(decoder.get(), buffer_index, 0,
                                                         request.decode_aac_request.size, 0, 0);
    if (status != AMEDIA_OK) {
        LOG_WARNING(Audio_DSP, "Try queue input buffer again later!");
        return response;
    }

    // output
    AMediaCodecBufferInfo info;
    std::array<std::vector<u16>, 2> out_streams;
    buffer_index = AMediaCodec_dequeueOutputBuffer(decoder.get(), &info, timeout);
    switch (buffer_index) {
    case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
        LOG_WARNING(Audio_DSP, "Failed to dequeue output buffer: timeout!");
        break;
    case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
        LOG_WARNING(Audio_DSP, "Failed to dequeue output buffer: buffers changed!");
        break;
    case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED: {
        AMediaFormat* format = AMediaCodec_getOutputFormat(decoder.get());
        LOG_WARNING(Audio_DSP, "output format: {}", AMediaFormat_toString(format));
        AMediaFormat_delete(format);
        buffer_index = AMediaCodec_dequeueOutputBuffer(decoder.get(), &info, timeout);
    }
    default: {
        int offset = info.offset;
        buffer = AMediaCodec_getOutputBuffer(decoder.get(), buffer_index, &buffer_size);
        while (offset < info.size) {
            for (int channel = 0; channel < response.decode_aac_response.num_channels; channel++) {
                u16 pcm_data;
                std::memcpy(&pcm_data, buffer + offset, sizeof(pcm_data));
                out_streams[channel].push_back(pcm_data);
                offset += sizeof(pcm_data);
            }
        }
        AMediaCodec_releaseOutputBuffer(decoder.get(), buffer_index, info.size != 0);
    }
    }

    // transfer the decoded buffer from vector to the FCRAM
    size_t stream0_size = out_streams[0].size() * sizeof(u16);
    if (stream0_size != 0) {
        if (request.decode_aac_request.dst_addr_ch0 < Memory::FCRAM_PADDR ||
            request.decode_aac_request.dst_addr_ch0 + stream0_size >
                Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
            LOG_ERROR(Audio_DSP, "Got out of bounds dst_addr_ch0 {:08x}",
                      request.decode_aac_request.dst_addr_ch0);
            return response;
        }
        std::memcpy(
            memory.GetFCRAMPointer(request.decode_aac_request.dst_addr_ch0 - Memory::FCRAM_PADDR),
            out_streams[0].data(), stream0_size);
    }

    size_t stream1_size = out_streams[1].size() * sizeof(u16);
    if (stream1_size != 0) {
        if (request.decode_aac_request.dst_addr_ch1 < Memory::FCRAM_PADDR ||
            request.decode_aac_request.dst_addr_ch1 + stream1_size >
                Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
            LOG_ERROR(Audio_DSP, "Got out of bounds dst_addr_ch1 {:08x}",
                      request.decode_aac_request.dst_addr_ch1);
            return response;
        }
        std::memcpy(
            memory.GetFCRAMPointer(request.decode_aac_request.dst_addr_ch1 - Memory::FCRAM_PADDR),
            out_streams[1].data(), stream1_size);
    }
    return response;
}

MediaNDKDecoder::MediaNDKDecoder(Memory::MemorySystem& memory)
    : impl(std::make_unique<Impl>(memory)) {}

MediaNDKDecoder::~MediaNDKDecoder() = default;

std::optional<BinaryMessage> MediaNDKDecoder::ProcessRequest(const BinaryMessage& request) {
    return impl->ProcessRequest(request);
}

bool MediaNDKDecoder::IsValid() const {
    return true;
}

} // namespace AudioCore::HLE
