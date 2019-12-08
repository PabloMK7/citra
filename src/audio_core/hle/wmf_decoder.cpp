// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/hle/wmf_decoder.h"
#include "audio_core/hle/wmf_decoder_utils.h"

namespace AudioCore::HLE {

using namespace MFDecoder;

class WMFDecoder::Impl {
public:
    explicit Impl(Memory::MemorySystem& memory);
    ~Impl();
    std::optional<BinaryResponse> ProcessRequest(const BinaryRequest& request);
    bool IsValid() const {
        return is_valid;
    }

private:
    std::optional<BinaryResponse> Initalize(const BinaryRequest& request);

    std::optional<BinaryResponse> Decode(const BinaryRequest& request);

    MFOutputState DecodingLoop(ADTSData adts_header, std::array<std::vector<u8>, 2>& out_streams);

    bool transform_initialized = false;
    bool format_selected = false;

    Memory::MemorySystem& memory;

    unique_mfptr<IMFTransform> transform;
    DWORD in_stream_id = 0;
    DWORD out_stream_id = 0;
    bool is_valid = false;
    bool mf_started = false;
    bool coinited = false;
};

WMFDecoder::Impl::Impl(Memory::MemorySystem& memory) : memory(memory) {
    // Attempt to load the symbols for mf.dll
    if (!InitMFDLL()) {
        LOG_CRITICAL(Audio_DSP,
                     "Unable to load mf.dll. AAC audio through media foundation unavailable");
        return;
    }

    HRESULT hr = S_OK;
    hr = CoInitialize(NULL);
    // S_FALSE will be returned when COM has already been initialized
    if (hr != S_OK && hr != S_FALSE) {
        ReportError("Failed to start COM components", hr);
    } else {
        coinited = true;
    }

    // lite startup is faster and all what we need is included
    hr = MFDecoder::MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (hr != S_OK) {
        // Do you know you can't initialize MF in test mode or safe mode?
        ReportError("Failed to initialize Media Foundation", hr);
    } else {
        mf_started = true;
    }

    LOG_INFO(Audio_DSP, "Media Foundation activated");

    // initialize transform
    transform = MFDecoderInit();
    if (transform == nullptr) {
        LOG_CRITICAL(Audio_DSP, "Can't initialize decoder");
        return;
    }

    hr = transform->GetStreamIDs(1, &in_stream_id, 1, &out_stream_id);
    if (hr == E_NOTIMPL) {
        // if not implemented, it means this MFT does not assign stream ID for you
        in_stream_id = 0;
        out_stream_id = 0;
    } else if (FAILED(hr)) {
        ReportError("Decoder failed to initialize the stream ID", hr);
        return;
    }
    transform_initialized = true;
    is_valid = true;
}

WMFDecoder::Impl::~Impl() {
    if (transform_initialized) {
        MFFlush(transform.get());
        // delete the transform object before shutting down MF
        // otherwise access violation will occur
        transform.reset();
    }
    if (mf_started) {
        MFDecoder::MFShutdown();
    }
    if (coinited) {
        CoUninitialize();
    }
}

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
    BinaryResponse response;
    std::memcpy(&response, &request, sizeof(response));
    response.unknown1 = 0x0;

    format_selected = false; // select format again if application request initialize the DSP
    return response;
}

MFOutputState WMFDecoder::Impl::DecodingLoop(ADTSData adts_header,
                                             std::array<std::vector<u8>, 2>& out_streams) {
    MFOutputState output_status = MFOutputState::OK;
    std::optional<std::vector<f32>> output_buffer;
    unique_mfptr<IMFSample> output;

    while (true) {
        auto [output_status, output] = ReceiveSample(transform.get(), out_stream_id);

        // 0 -> okay; 3 -> okay but more data available (buffer too small)
        if (output_status == MFOutputState::OK || output_status == MFOutputState::HaveMoreData) {
            output_buffer = CopySampleToBuffer(output.get());

            // the following was taken from ffmpeg version of the decoder
            f32 val_f32;
            for (std::size_t i = 0; i < output_buffer->size();) {
                for (std::size_t channel = 0; channel < adts_header.channels; channel++) {
                    val_f32 = std::clamp(output_buffer->at(i), -1.0f, 1.0f);
                    s16 val = static_cast<s16>(0x7FFF * val_f32);
                    out_streams[channel].push_back(val & 0xFF);
                    out_streams[channel].push_back(val >> 8);
                    // i is incremented on per channel basis
                    i++;
                }
            }
        }

        // in case of "ok" only, just return quickly
        if (output_status == MFOutputState::OK)
            return MFOutputState::OK;

        // for status = 2, reset MF
        if (output_status == MFOutputState::NeedReconfig) {
            format_selected = false;
            return MFOutputState::NeedReconfig;
        }

        // for status = 3, try again with new buffer
        if (output_status == MFOutputState::HaveMoreData)
            continue;

        // according to MS document, this is not an error (?!)
        if (output_status == MFOutputState::NeedMoreInput)
            return MFOutputState::NeedMoreInput;

        return MFOutputState::FatalError; // return on other status
    }

    return MFOutputState::FatalError;
}

std::optional<BinaryResponse> WMFDecoder::Impl::Decode(const BinaryRequest& request) {
    BinaryResponse response;
    response.codec = request.codec;
    response.cmd = request.cmd;
    response.size = request.size;
    response.num_channels = 2;
    response.num_samples = 1024;

    if (!transform_initialized) {
        LOG_DEBUG(Audio_DSP, "Decoder not initialized");
        // This is a hack to continue games when decoder failed to initialize
        return response;
    }

    if (request.src_addr < Memory::FCRAM_PADDR ||
        request.src_addr + request.size > Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
        LOG_ERROR(Audio_DSP, "Got out of bounds src_addr {:08x}", request.src_addr);
        return {};
    }
    u8* data = memory.GetFCRAMPointer(request.src_addr - Memory::FCRAM_PADDR);

    std::array<std::vector<u8>, 2> out_streams;
    unique_mfptr<IMFSample> sample;
    MFInputState input_status = MFInputState::OK;
    MFOutputState output_status = MFOutputState::OK;
    std::optional<ADTSMeta> adts_meta = DetectMediaType((char*)data, request.size);

    if (!adts_meta) {
        LOG_ERROR(Audio_DSP, "Unable to deduce decoding parameters from ADTS stream");
        return response;
    }

    response.num_channels = adts_meta->ADTSHeader.channels;

    if (!format_selected) {
        LOG_DEBUG(Audio_DSP, "New ADTS stream: channels = {}, sample rate = {}",
                  adts_meta->ADTSHeader.channels, adts_meta->ADTSHeader.samplerate);
        SelectInputMediaType(transform.get(), in_stream_id, adts_meta->ADTSHeader,
                             adts_meta->AACTag, 14);
        SelectOutputMediaType(transform.get(), out_stream_id);
        SendSample(transform.get(), in_stream_id, nullptr);
        // cache the result from detect_mediatype and call select_*_mediatype only once
        // This could increase performance very slightly
        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        format_selected = true;
    }

    sample = CreateSample((void*)data, request.size, 1, 0);
    sample->SetUINT32(MFSampleExtension_CleanPoint, 1);

    while (true) {
        input_status = SendSample(transform.get(), in_stream_id, sample.get());
        output_status = DecodingLoop(adts_meta->ADTSHeader, out_streams);

        if (output_status == MFOutputState::FatalError) {
            // if the decode issues are caused by MFT not accepting new samples, try again
            // NOTICE: you are required to check the output even if you already knew/guessed
            // MFT didn't accept the input sample
            if (input_status == MFInputState::NotAccepted) {
                // try again
                continue;
            }

            LOG_ERROR(Audio_DSP, "Errors occurred when receiving output");
            return response;
        } else if (output_status == MFOutputState::NeedReconfig) {
            // flush the transform
            MFFlush(transform.get());
            // decode again
            return this->Decode(request);
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

bool WMFDecoder::IsValid() const {
    return impl->IsValid();
}

} // namespace AudioCore::HLE
