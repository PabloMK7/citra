// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/hle/aac_decoder.h"
#include "audio_core/hle/ffmpeg_dl.h"

namespace AudioCore::HLE {

class AACDecoder::Impl {
public:
    explicit Impl(Memory::MemorySystem& memory);
    ~Impl();
    std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request);

private:
    std::optional<BinaryResponse> Initalize(const BinaryRequest& request);

    void Clear();

    std::optional<BinaryResponse> Decode(const BinaryRequest& request);

    bool initalized;
    bool have_ffmpeg_dl;

    Memory::MemorySystem& memory;

    AVCodec* codec;
    AVCodecContext* av_context = nullptr;
    AVCodecParserContext* parser = nullptr;
    AVPacket* av_packet;
    AVFrame* decoded_frame = nullptr;
};

AACDecoder::Impl::Impl(Memory::MemorySystem& memory) : memory(memory) {
    initalized = false;

    have_ffmpeg_dl = InitFFmpegDL();
}

AACDecoder::Impl::~Impl() {
    if (initalized)
        Clear();
}

std::optional<BinaryResponse> AACDecoder::Impl::ProcessRequest(const BinaryRequest& request) {
    if (request.codec != DecoderCodec::AAC) {
        LOG_ERROR(Audio_DSP, "Got wrong codec {}", static_cast<u16>(request.codec));
        return {};
    }

    switch (request.cmd) {
    case DecoderCommand::Init: {
        return Initalize(request);
        break;
    }
    case DecoderCommand::Decode: {
        return Decode(request);
        break;
    }
    case DecoderCommand::Unknown: {
        BinaryResponse response;
        std::memcpy(&response, &request, sizeof(response));
        response.unknown1 = 0x0;
        return response;
        break;
    }
    default:
        LOG_ERROR(Audio_DSP, "Got unknown binary request: {}", static_cast<u16>(request.cmd));
        return {};
        break;
    }
}

std::optional<BinaryResponse> AACDecoder::Impl::Initalize(const BinaryRequest& request) {
    if (initalized) {
        Clear();
    }

    BinaryResponse response;
    std::memcpy(&response, &request, sizeof(response));
    response.unknown1 = 0x0;

    if (!have_ffmpeg_dl) {
        return response;
    }

    av_packet = av_packet_alloc_dl();

    codec = avcodec_find_decoder_dl(AV_CODEC_ID_AAC);
    if (!codec) {
        LOG_ERROR(Audio_DSP, "Codec not found\n");
        return response;
    }

    parser = av_parser_init_dl(codec->id);
    if (!parser) {
        LOG_ERROR(Audio_DSP, "Parser not found\n");
        return response;
    }

    av_context = avcodec_alloc_context3_dl(codec);
    if (!av_context) {
        LOG_ERROR(Audio_DSP, "Could not allocate audio codec context\n");
        return response;
    }

    if (avcodec_open2_dl(av_context, codec, NULL) < 0) {
        LOG_ERROR(Audio_DSP, "Could not open codec\n");
        return response;
    }

    initalized = true;
    return response;
}

void AACDecoder::Impl::Clear() {
    if (!have_ffmpeg_dl) {
        return;
    }

    avcodec_free_context_dl(&av_context);
    av_parser_close_dl(parser);
    av_frame_free_dl(&decoded_frame);
    av_packet_free_dl(&av_packet);
}

std::optional<BinaryResponse> AACDecoder::Impl::Decode(const BinaryRequest& request) {
    if (!initalized) {
        LOG_DEBUG(Audio_DSP, "Decoder not initalized");

        // This is a hack to continue games that are not compiled with the aac codec
        BinaryResponse response;
        response.codec = request.codec;
        response.cmd = request.cmd;
        response.num_channels = 2;
        response.num_samples = 1024;
        response.size = request.size;
        return response;
    }

    if (request.src_addr < Memory::FCRAM_PADDR ||
        request.src_addr + request.size > Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
        LOG_ERROR(Audio_DSP, "Got out of bounds src_addr {:08x}", request.src_addr);
        return {};
    }
    u8* data = memory.GetFCRAMPointer(request.src_addr - Memory::FCRAM_PADDR);

    std::array<std::vector<u8>, 2> out_streams;

    std::size_t data_size = request.size;
    while (data_size > 0) {
        if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc_dl())) {
                LOG_ERROR(Audio_DSP, "Could not allocate audio frame");
                return {};
            }
        }

        int ret = av_parser_parse2_dl(parser, av_context, &av_packet->data, &av_packet->size, data,
                                      data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) {
            LOG_ERROR(Audio_DSP, "Error while parsing");
            return {};
        }
        data += ret;
        data_size -= ret;

        ret = avcodec_send_packet_dl(av_context, av_packet);
        if (ret < 0) {
            LOG_ERROR(Audio_DSP, "Error submitting the packet to the decoder");
            return {};
        }

        if (av_packet->size) {
            while (ret >= 0) {
                ret = avcodec_receive_frame_dl(av_context, decoded_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else if (ret < 0) {
                    LOG_ERROR(Audio_DSP, "Error during decoding");
                    return {};
                }
                int bytes_per_sample = av_get_bytes_per_sample_dl(av_context->sample_fmt);
                if (bytes_per_sample < 0) {
                    LOG_ERROR(Audio_DSP, "Failed to calculate data size");
                    return {};
                }

                ASSERT(decoded_frame->channels == out_streams.size());

                std::size_t size = bytes_per_sample * (decoded_frame->nb_samples);

                // FFmpeg converts to 32 signed floating point PCM, we need s16 PCM so we need to
                // convert it
                f32 val_float;
                for (std::size_t current_pos(0); current_pos < size;) {
                    for (std::size_t channel(0); channel < out_streams.size(); channel++) {
                        std::memcpy(&val_float, decoded_frame->data[0] + current_pos,
                                    sizeof(val_float));
                        s16 val = static_cast<s16>(0x7FFF * val_float);
                        out_streams[channel].push_back(val & 0xFF);
                        out_streams[channel].push_back(val >> 8);
                    }
                    current_pos += sizeof(val_float);
                }
            }
        }
    }

    if (request.dst_addr_ch0 < Memory::FCRAM_PADDR ||
        request.dst_addr_ch0 + out_streams[0].size() > Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
        LOG_ERROR(Audio_DSP, "Got out of bounds dst_addr_ch0 {:08x}", request.dst_addr_ch0);
        return {};
    }
    std::memcpy(memory.GetFCRAMPointer(request.dst_addr_ch0 - Memory::FCRAM_PADDR),
                out_streams[0].data(), out_streams[0].size());

    if (request.dst_addr_ch1 < Memory::FCRAM_PADDR ||
        request.dst_addr_ch1 + out_streams[1].size() > Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
        LOG_ERROR(Audio_DSP, "Got out of bounds dst_addr_ch1 {:08x}", request.dst_addr_ch1);
        return {};
    }
    std::memcpy(memory.GetFCRAMPointer(request.dst_addr_ch1 - Memory::FCRAM_PADDR),
                out_streams[1].data(), out_streams[1].size());

    BinaryResponse response;
    response.codec = request.codec;
    response.cmd = request.cmd;
    response.num_channels = 2;
    response.num_samples = decoded_frame->nb_samples;
    response.size = request.size;
    return response;
}

AACDecoder::AACDecoder(Memory::MemorySystem& memory) : impl(std::make_unique<Impl>(memory)) {}

AACDecoder::~AACDecoder() = default;

std::optional<BinaryResponse> AACDecoder::ProcessRequest(const BinaryRequest& request) {
    return impl->ProcessRequest(request);
}

} // namespace AudioCore::HLE
