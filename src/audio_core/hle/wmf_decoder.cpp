// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/hle/wmf_decoder.h"
#include "audio_core/hle/wmf_decoder_utils.h"

namespace AudioCore::HLE {

class WMFDecoder::Impl {
public:
    explicit Impl(Memory::MemorySystem& memory);
    ~Impl();
    std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request);

private:
    std::optional<BinaryResponse> Initalize(const BinaryRequest& request);

    void Clear();

    std::optional<BinaryResponse> Decode(const BinaryRequest& request);

    int DecodingLoop(ADTSData adts_header, std::array<std::vector<u8>, 2>& out_streams);

    bool initalized = false;
    bool selected = false;

    Memory::MemorySystem& memory;

    IMFTransform* transform = nullptr;
    DWORD in_stream_id = 0;
    DWORD out_stream_id = 0;
};

WMFDecoder::Impl::Impl(Memory::MemorySystem& memory) : memory(memory) {
    MFCoInit();
}

WMFDecoder::Impl::~Impl() = default;

std::optional<BinaryResponse> WMFDecoder::Impl::ProcessRequest(const BinaryRequest& request) {
    if (request.codec != DecoderCodec::AAC) {
        LOG_ERROR(Audio_DSP, "Got unknown codec {}", static_cast<u16>(request.codec));
        return {};
    }

    switch (request.cmd) {
    case DecoderCommand::Init: {
        LOG_INFO(Audio_DSP, "WMFDecoder initializing");
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

std::optional<BinaryResponse> WMFDecoder::Impl::Initalize(const BinaryRequest& request) {
    if (initalized) {
        Clear();
    }

    BinaryResponse response;
    std::memcpy(&response, &request, sizeof(response));
    response.unknown1 = 0x0;

    if (!MFDecoderInit(&transform)) {
        LOG_CRITICAL(Audio_DSP, "Can't init decoder");
        return response;
    }

    HRESULT hr = transform->GetStreamIDs(1, &in_stream_id, 1, &out_stream_id);
    if (hr == E_NOTIMPL) {
        // if not implemented, it means this MFT does not assign stream ID for you
        in_stream_id = 0;
        out_stream_id = 0;
    } else if (FAILED(hr)) {
        ReportError("Decoder failed to initialize the stream ID", hr);
        SafeRelease(&transform);
        return response;
    }

    initalized = true;
    return response;
}

void WMFDecoder::Impl::Clear() {
    if (initalized) {
        MFFlush(&transform);
        MFDeInit(&transform);
    }
    initalized = false;
    selected = false;
}

int WMFDecoder::Impl::DecodingLoop(ADTSData adts_header,
                                   std::array<std::vector<u8>, 2>& out_streams) {
    MFOutputState output_status = OK;
    char* output_buffer = nullptr;
    DWORD output_len = 0;
    IMFSample* output = nullptr;

    while (true) {
        output_status = ReceiveSample(transform, out_stream_id, &output);

        // 0 -> okay; 3 -> okay but more data available (buffer too small)
        if (output_status == OK || output_status == HAVE_MORE_DATA) {
            CopySampleToBuffer(output, (void**)&output_buffer, &output_len);

            // the following was taken from ffmpeg version of the decoder
            f32 val_f32;
            for (size_t i = 0; i < output_len;) {
                for (std::size_t channel = 0; channel < adts_header.channels; channel++) {
                    std::memcpy(&val_f32, output_buffer + i, sizeof(val_f32));
                    s16 val = static_cast<s16>(0x7FFF * val_f32);
                    out_streams[channel].push_back(val & 0xFF);
                    out_streams[channel].push_back(val >> 8);
                    i += sizeof(val_f32);
                }
            }

            if (output_buffer)
                free(output_buffer);
        }

        // in case of "ok" only, just return quickly
        if (output_status == OK)
            return 0;

        // for status = 2, reset MF
        if (output_status == NEED_RECONFIG) {
            Clear();
            return -1;
        }

        // for status = 3, try again with new buffer
        if (output_status == HAVE_MORE_DATA)
            continue;

        if (output_status == NEED_MORE_INPUT) // according to MS document, this is not an error (?!)
            return 1;

        return -1; // return on other status
    }

    return -1;
}

std::optional<BinaryResponse> WMFDecoder::Impl::Decode(const BinaryRequest& request) {
    BinaryResponse response;
    response.codec = request.codec;
    response.cmd = request.cmd;
    response.size = request.size;
    response.num_channels = 2;
    response.num_samples = 1024;

    if (!initalized) {
        LOG_DEBUG(Audio_DSP, "Decoder not initalized");
        // This is a hack to continue games that are not compiled with the aac codec
        return response;
    }

    if (request.src_addr < Memory::FCRAM_PADDR ||
        request.src_addr + request.size > Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
        LOG_ERROR(Audio_DSP, "Got out of bounds src_addr {:08x}", request.src_addr);
        return {};
    }
    u8* data = memory.GetFCRAMPointer(request.src_addr - Memory::FCRAM_PADDR);

    std::array<std::vector<u8>, 2> out_streams;
    IMFSample* sample = nullptr;
    ADTSData adts_header;
    char* aac_tag = (char*)calloc(1, 14);
    int input_status = 0;

    if (DetectMediaType((char*)data, request.size, &adts_header, &aac_tag) != 0) {
        LOG_ERROR(Audio_DSP, "Unable to deduce decoding parameters from ADTS stream");
        return response;
    }

    response.num_channels = adts_header.channels;

    if (!selected) {
        LOG_DEBUG(Audio_DSP, "New ADTS stream: channels = {}, sample rate = {}",
                  adts_header.channels, adts_header.samplerate);
        SelectInputMediaType(transform, in_stream_id, adts_header, (UINT8*)aac_tag, 14);
        SelectOutputMediaType(transform, out_stream_id);
        SendSample(transform, in_stream_id, nullptr);
        // cache the result from detect_mediatype and call select_*_mediatype only once
        // This could increase performance very slightly
        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        selected = true;
    }

    sample = CreateSample((void*)data, request.size, 1, 0);
    sample->SetUINT32(MFSampleExtension_CleanPoint, 1);

    while (true) {
        input_status = SendSample(transform, in_stream_id, sample);

        if (DecodingLoop(adts_header, out_streams) < 0) {
            // if the decode issues are caused by MFT not accepting new samples, try again
            // NOTICE: you are required to check the output even if you already knew/guessed
            // MFT didn't accept the input sample
            if (input_status == 1) {
                // try again
                continue;
            }

            LOG_ERROR(Audio_DSP, "Errors occurred when receiving output");
            return response;
        }

        break; // jump out of the loop if at least we don't have obvious issues
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

WMFDecoder::WMFDecoder(Memory::MemorySystem& memory) : impl(std::make_unique<Impl>(memory)) {}

WMFDecoder::~WMFDecoder() = default;

std::optional<BinaryResponse> WMFDecoder::ProcessRequest(const BinaryRequest& request) {
    return impl->ProcessRequest(request);
}

} // namespace AudioCore::HLE
