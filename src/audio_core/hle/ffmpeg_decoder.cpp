// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/hle/ffmpeg_decoder.h"
#include "audio_core/hle/ffmpeg_dl.h"

namespace AudioCore::HLE {

class FFMPEGDecoder::Impl {
public:
    explicit Impl(Memory::MemorySystem& memory);
    ~Impl();
    std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request);
    bool IsValid() const {
        return initalized;
    }

private:
    std::optional<BinaryResponse> Initalize(const BinaryRequest& request);

    void Clear();

    std::optional<BinaryResponse> Decode(const BinaryRequest& request);

    struct AVPacketDeleter {
        void operator()(AVPacket* packet) const {
            av_packet_free_dl(&packet);
        }
    };

    struct AVCodecContextDeleter {
        void operator()(AVCodecContext* context) const {
            avcodec_free_context_dl(&context);
        }
    };

    struct AVCodecParserContextDeleter {
        void operator()(AVCodecParserContext* parser) const {
            av_parser_close_dl(parser);
        }
    };

    struct AVFrameDeleter {
        void operator()(AVFrame* frame) const {
            av_frame_free_dl(&frame);
        }
    };

    bool initalized = false;
    bool have_ffmpeg_dl;

    Memory::MemorySystem& memory;

    AVCodec* codec;
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> av_context;
    std::unique_ptr<AVCodecParserContext, AVCodecParserContextDeleter> parser;
    std::unique_ptr<AVPacket, AVPacketDeleter> av_packet;
    std::unique_ptr<AVFrame, AVFrameDeleter> decoded_frame;
};

FFMPEGDecoder::Impl::Impl(Memory::MemorySystem& memory) : memory(memory) {
    have_ffmpeg_dl = InitFFmpegDL();
}

FFMPEGDecoder::Impl::~Impl() = default;

std::optional<BinaryResponse> FFMPEGDecoder::Impl::ProcessRequest(const BinaryRequest& request) {
    if (request.codec != DecoderCodec::AAC) {
        LOG_ERROR(Audio_DSP, "Got wrong codec {}", static_cast<u16>(request.codec));
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

std::optional<BinaryResponse> FFMPEGDecoder::Impl::Initalize(const BinaryRequest& request) {
    if (initalized) {
        Clear();
    }

    BinaryResponse response;
    std::memcpy(&response, &request, sizeof(response));
    response.unknown1 = 0x0;

    if (!have_ffmpeg_dl) {
        return response;
    }

    av_packet.reset(av_packet_alloc_dl());

    codec = avcodec_find_decoder_dl(AV_CODEC_ID_AAC);
    if (!codec) {
        LOG_ERROR(Audio_DSP, "Codec not found\n");
        return response;
    }

    parser.reset(av_parser_init_dl(codec->id));
    if (!parser) {
        LOG_ERROR(Audio_DSP, "Parser not found\n");
        return response;
    }

    av_context.reset(avcodec_alloc_context3_dl(codec));
    if (!av_context) {
        LOG_ERROR(Audio_DSP, "Could not allocate audio codec context\n");
        return response;
    }

    if (avcodec_open2_dl(av_context.get(), codec, nullptr) < 0) {
        LOG_ERROR(Audio_DSP, "Could not open codec\n");
        return response;
    }

    initalized = true;
    return response;
}

void FFMPEGDecoder::Impl::Clear() {
    if (!have_ffmpeg_dl) {
        return;
    }

    av_context.reset();
    parser.reset();
    decoded_frame.reset();
    av_packet.reset();
}

std::optional<BinaryResponse> FFMPEGDecoder::Impl::Decode(const BinaryRequest& request) {
    BinaryResponse response;
    response.codec = request.codec;
    response.cmd = request.cmd;
    response.size = request.size;

    if (!initalized) {
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

    std::array<std::vector<u8>, 2> out_streams;

    std::size_t data_size = request.size;
    while (data_size > 0) {
        if (!decoded_frame) {
            decoded_frame.reset(av_frame_alloc_dl());
            if (!decoded_frame) {
                LOG_ERROR(Audio_DSP, "Could not allocate audio frame");
                return {};
            }
        }

        int ret =
            av_parser_parse2_dl(parser.get(), av_context.get(), &av_packet->data, &av_packet->size,
                                data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) {
            LOG_ERROR(Audio_DSP, "Error while parsing");
            return {};
        }
        data += ret;
        data_size -= ret;

        ret = avcodec_send_packet_dl(av_context.get(), av_packet.get());
        if (ret < 0) {
            LOG_ERROR(Audio_DSP, "Error submitting the packet to the decoder");
            return {};
        }

        if (av_packet->size) {
            while (ret >= 0) {
                ret = avcodec_receive_frame_dl(av_context.get(), decoded_frame.get());
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

                ASSERT(decoded_frame->channels <= out_streams.size());

                std::size_t size = bytes_per_sample * (decoded_frame->nb_samples);

                response.num_channels = decoded_frame->channels;
                response.num_samples += decoded_frame->nb_samples;

                // FFmpeg converts to 32 signed floating point PCM, we need s16 PCM so we need to
                // convert it
                f32 val_float;
                for (std::size_t current_pos(0); current_pos < size;) {
                    for (std::size_t channel(0); channel < decoded_frame->channels; channel++) {
                        std::memcpy(&val_float, decoded_frame->data[channel] + current_pos,
                                    sizeof(val_float));
                        val_float = std::clamp(val_float, -1.0f, 1.0f);
                        s16 val = static_cast<s16>(0x7FFF * val_float);
                        out_streams[channel].push_back(val & 0xFF);
                        out_streams[channel].push_back(val >> 8);
                    }
                    current_pos += sizeof(val_float);
                }
            }
        }
    }

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

FFMPEGDecoder::FFMPEGDecoder(Memory::MemorySystem& memory) : impl(std::make_unique<Impl>(memory)) {}

FFMPEGDecoder::~FFMPEGDecoder() = default;

std::optional<BinaryResponse> FFMPEGDecoder::ProcessRequest(const BinaryRequest& request) {
    return impl->ProcessRequest(request);
}

bool FFMPEGDecoder::IsValid() const {
    return impl->IsValid();
}

} // namespace AudioCore::HLE
