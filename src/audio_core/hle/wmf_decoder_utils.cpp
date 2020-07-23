// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#include "common/logging/log.h"
#include "common/string_util.h"
#include "wmf_decoder_utils.h"

namespace MFDecoder {

// utility functions
void ReportError(std::string msg, HRESULT hr) {
    if (SUCCEEDED(hr)) {
        return;
    }
    LPWSTR err;
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, hr,
                   // hardcode to use en_US because if any user had problems with this
                   // we can help them w/o translating anything
                   // default is to use the language currently active on the operating system
                   MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPWSTR)&err, 0, nullptr);
    if (err != nullptr) {
        LOG_CRITICAL(Audio_DSP, "{}: {}", msg, Common::UTF16ToUTF8(err));
        LocalFree(err);
    }
    LOG_CRITICAL(Audio_DSP, "{}: {:08x}", msg, hr);
}

unique_mfptr<IMFTransform> MFDecoderInit(GUID audio_format) {

    HRESULT hr = S_OK;
    MFT_REGISTER_TYPE_INFO reg = {0};
    GUID category = MFT_CATEGORY_AUDIO_DECODER;
    IMFActivate** activate;
    unique_mfptr<IMFTransform> transform;
    UINT32 num_activate;

    reg.guidMajorType = MFMediaType_Audio;
    reg.guidSubtype = audio_format;

    hr = MFTEnumEx(category,
                   MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER,
                   &reg, nullptr, &activate, &num_activate);
    if (FAILED(hr) || num_activate < 1) {
        ReportError("Failed to enumerate decoders", hr);
        CoTaskMemFree(activate);
        return nullptr;
    }
    LOG_INFO(Audio_DSP, "Windows(R) Media Foundation found {} suitable decoder(s)", num_activate);
    for (unsigned int n = 0; n < num_activate; n++) {
        hr = activate[n]->ActivateObject(
            IID_IMFTransform,
            reinterpret_cast<void**>(static_cast<IMFTransform**>(Amp(transform))));
        if (FAILED(hr))
            transform = nullptr;
        activate[n]->Release();
        if (SUCCEEDED(hr))
            break;
    }
    if (transform == nullptr) {
        ReportError("Failed to initialize MFT", hr);
        CoTaskMemFree(activate);
        return nullptr;
    }
    CoTaskMemFree(activate);
    return transform;
}

unique_mfptr<IMFSample> CreateSample(const void* data, DWORD len, DWORD alignment,
                                     LONGLONG duration) {
    HRESULT hr = S_OK;
    unique_mfptr<IMFMediaBuffer> buf;
    unique_mfptr<IMFSample> sample;

    hr = MFCreateSample(Amp(sample));
    if (FAILED(hr)) {
        ReportError("Unable to allocate a sample", hr);
        return nullptr;
    }
    // Yes, the argument for alignment is the actual alignment - 1
    hr = MFCreateAlignedMemoryBuffer(len, alignment - 1, Amp(buf));
    if (FAILED(hr)) {
        ReportError("Unable to allocate a memory buffer for sample", hr);
        return nullptr;
    }
    if (data) {
        BYTE* buffer;
        // lock the MediaBuffer
        // this is actually not a thread-safe lock
        hr = buf->Lock(&buffer, nullptr, nullptr);
        if (FAILED(hr)) {
            ReportError("Unable to lock down MediaBuffer", hr);
            return nullptr;
        }

        std::memcpy(buffer, data, len);

        buf->SetCurrentLength(len);
        buf->Unlock();
    }

    sample->AddBuffer(buf.get());
    hr = sample->SetSampleDuration(duration);
    if (FAILED(hr)) {
        // MFT will take a guess for you in this case
        ReportError("Unable to set sample duration, but continuing anyway", hr);
    }

    return sample;
}

bool SelectInputMediaType(IMFTransform* transform, int in_stream_id, const ADTSData& adts,
                          const UINT8* user_data, UINT32 user_data_len, GUID audio_format) {
    HRESULT hr = S_OK;
    unique_mfptr<IMFMediaType> t;

    // actually you can get rid of the whole block of searching and filtering mess
    // if you know the exact parameters of your media stream
    hr = MFCreateMediaType(Amp(t));
    if (FAILED(hr)) {
        ReportError("Unable to create an empty MediaType", hr);
        return false;
    }

    // basic definition
    t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    t->SetGUID(MF_MT_SUBTYPE, audio_format);

    t->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1);
    t->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, adts.channels);
    t->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, adts.samplerate);
    // 0xfe = 254 = "unspecified"
    t->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 254);
    t->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
    t->SetBlob(MF_MT_USER_DATA, user_data, user_data_len);
    hr = transform->SetInputType(in_stream_id, t.get(), 0);
    if (FAILED(hr)) {
        ReportError("failed to select input types for MFT", hr);
        return false;
    }

    return true;
}

bool SelectOutputMediaType(IMFTransform* transform, int out_stream_id, GUID audio_format) {
    HRESULT hr = S_OK;
    UINT32 tmp;
    unique_mfptr<IMFMediaType> type;

    // If you know what you need and what you are doing, you can specify the conditions instead of
    // searching but it's better to use search since MFT may or may not support your output
    // parameters
    for (DWORD i = 0;; i++) {
        hr = transform->GetOutputAvailableType(out_stream_id, i, Amp(type));
        if (hr == MF_E_NO_MORE_TYPES || hr == E_NOTIMPL) {
            return true;
        }
        if (FAILED(hr)) {
            ReportError("failed to get output types for MFT", hr);
            return false;
        }

        hr = type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &tmp);

        if (FAILED(hr))
            continue;
        // select PCM-16 format
        if (tmp == 32) {
            hr = type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
            if (FAILED(hr)) {
                ReportError("failed to set MF_MT_AUDIO_BLOCK_ALIGNMENT for MFT on output stream",
                            hr);
                return false;
            }
            hr = transform->SetOutputType(out_stream_id, type.get(), 0);
            if (FAILED(hr)) {
                ReportError("failed to select output types for MFT", hr);
                return false;
            }
            return true;
        } else {
            continue;
        }

        return false;
    }

    ReportError("MFT: Unable to find preferred output format", E_NOTIMPL);
    return false;
}

std::optional<ADTSMeta> DetectMediaType(char* buffer, std::size_t len) {
    if (len < 7) {
        return std::nullopt;
    }

    ADTSData tmp;
    ADTSMeta result;
    // see https://docs.microsoft.com/en-us/windows/desktop/api/mmreg/ns-mmreg-heaacwaveinfo_tag
    // for the meaning of the byte array below

    // it might be a good idea to wrap the parameters into a struct
    // and pass that struct into the function but doing that will lead to messier code
    // const UINT8 aac_data[] = { 0x01, 0x00, 0xfe, 00, 00, 00, 00, 00, 00, 00, 00, 00, 0x11, 0x90
    // }; first byte: 0: raw aac 1: adts 2: adif 3: latm/laos
    UINT8 aac_tmp[] = {0x01, 0x00, 0xfe, 00, 00, 00, 00, 00, 00, 00, 00, 00, 0x00, 0x00};
    uint16_t tag = 0;

    tmp = ParseADTS(buffer);
    if (tmp.length == 0) {
        return std::nullopt;
    }

    tag = MFGetAACTag(tmp);
    aac_tmp[12] |= (tag & 0xff00) >> 8;
    aac_tmp[13] |= (tag & 0x00ff);
    std::memcpy(&(result.ADTSHeader), &tmp, sizeof(ADTSData));
    std::memcpy(&(result.AACTag), aac_tmp, 14);
    return result;
}

void MFFlush(IMFTransform* transform) {
    HRESULT hr = transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    if (FAILED(hr)) {
        ReportError("MFT: Flush command failed", hr);
    }
    hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
    if (FAILED(hr)) {
        ReportError("Failed to end streaming for MFT", hr);
    }
}

MFInputState SendSample(IMFTransform* transform, DWORD in_stream_id, IMFSample* in_sample) {
    HRESULT hr = S_OK;

    if (in_sample) {
        hr = transform->ProcessInput(in_stream_id, in_sample, 0);
        if (hr == MF_E_NOTACCEPTING) {
            return MFInputState::NotAccepted; // try again
        } else if (FAILED(hr)) {
            ReportError("MFT: Failed to process input", hr);
            return MFInputState::FatalError;
        } // FAILED(hr)
    } else {
        hr = transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        if (FAILED(hr)) {
            ReportError("MFT: Failed to drain when processing input", hr);
        }
    }

    return MFInputState::OK;
}

std::tuple<MFOutputState, unique_mfptr<IMFSample>> ReceiveSample(IMFTransform* transform,
                                                                 DWORD out_stream_id) {
    HRESULT hr;
    MFT_OUTPUT_DATA_BUFFER out_buffers;
    MFT_OUTPUT_STREAM_INFO out_info;
    DWORD status = 0;
    unique_mfptr<IMFSample> sample;
    bool mft_create_sample = false;

    hr = transform->GetOutputStreamInfo(out_stream_id, &out_info);

    if (FAILED(hr)) {
        ReportError("MFT: Failed to get stream info", hr);
        return std::make_tuple(MFOutputState::FatalError, std::move(sample));
    }
    mft_create_sample = (out_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) ||
                        (out_info.dwFlags & MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES);

    while (true) {
        status = 0;

        if (!mft_create_sample) {
            sample = CreateSample(nullptr, out_info.cbSize, out_info.cbAlignment);
            if (!sample.get()) {
                ReportError("MFT: Unable to allocate memory for samples", hr);
                return std::make_tuple(MFOutputState::FatalError, std::move(sample));
            }
        }

        out_buffers.dwStreamID = out_stream_id;
        out_buffers.pSample = sample.get();

        hr = transform->ProcessOutput(0, 1, &out_buffers, &status);

        if (!FAILED(hr)) {
            break;
        }

        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            // Most likely reasons: data corrupted; your actions not expected by MFT
            return std::make_tuple(MFOutputState::NeedMoreInput, std::move(sample));
        }

        if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
            ReportError("MFT: stream format changed, re-configuration required", hr);
            return std::make_tuple(MFOutputState::NeedReconfig, std::move(sample));
        }

        break;
    }

    if (out_buffers.dwStatus & MFT_OUTPUT_DATA_BUFFER_INCOMPLETE) {
        // this status is also unreliable but whatever
        return std::make_tuple(MFOutputState::HaveMoreData, std::move(sample));
    }

    if (out_buffers.pSample == nullptr) {
        ReportError("MFT: decoding failure", hr);
        return std::make_tuple(MFOutputState::FatalError, std::move(sample));
    }

    return std::make_tuple(MFOutputState::OK, std::move(sample));
}

std::optional<std::vector<f32>> CopySampleToBuffer(IMFSample* sample) {
    unique_mfptr<IMFMediaBuffer> buffer;
    HRESULT hr = S_OK;
    std::optional<std::vector<f32>> output;
    std::vector<f32> output_buffer;
    BYTE* data;
    DWORD len = 0;

    hr = sample->GetTotalLength(&len);
    if (FAILED(hr)) {
        ReportError("Failed to get the length of sample buffer", hr);
        return std::nullopt;
    }

    hr = sample->ConvertToContiguousBuffer(Amp(buffer));
    if (FAILED(hr)) {
        ReportError("Failed to get sample buffer", hr);
        return std::nullopt;
    }

    hr = buffer->Lock(&data, nullptr, nullptr);
    if (FAILED(hr)) {
        ReportError("Failed to lock the buffer", hr);
        return std::nullopt;
    }

    output_buffer.resize(len / sizeof(f32));
    std::memcpy(output_buffer.data(), data, len);
    output = output_buffer;

    // if buffer unlock fails, then... whatever, we have already got data
    buffer->Unlock();
    return output;
}

namespace {

struct LibraryDeleter {
    using pointer = HMODULE;
    void operator()(HMODULE h) const {
        if (h != nullptr)
            FreeLibrary(h);
    }
};

std::unique_ptr<HMODULE, LibraryDeleter> mf_dll{nullptr};
std::unique_ptr<HMODULE, LibraryDeleter> mfplat_dll{nullptr};

} // namespace

bool InitMFDLL() {

    mf_dll.reset(LoadLibrary(TEXT("mf.dll")));
    if (!mf_dll) {
        DWORD error_message_id = GetLastError();
        LPSTR message_buffer = nullptr;
        size_t size =
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

        std::string message(message_buffer, size);

        LocalFree(message_buffer);
        LOG_ERROR(Audio_DSP, "Could not load mf.dll: {}", message);
        return false;
    }

    mfplat_dll.reset(LoadLibrary(TEXT("mfplat.dll")));
    if (!mfplat_dll) {
        DWORD error_message_id = GetLastError();
        LPSTR message_buffer = nullptr;
        size_t size =
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

        std::string message(message_buffer, size);

        LocalFree(message_buffer);
        LOG_ERROR(Audio_DSP, "Could not load mfplat.dll: {}", message);
        return false;
    }

    MFStartup = Symbol<HRESULT(ULONG, DWORD)>(mfplat_dll.get(), "MFStartup");
    if (!MFStartup) {
        LOG_ERROR(Audio_DSP, "Cannot load function MFStartup");
        return false;
    }

    MFShutdown = Symbol<HRESULT(void)>(mfplat_dll.get(), "MFShutdown");
    if (!MFShutdown) {
        LOG_ERROR(Audio_DSP, "Cannot load function MFShutdown");
        return false;
    }

    MFShutdownObject = Symbol<HRESULT(IUnknown*)>(mf_dll.get(), "MFShutdownObject");
    if (!MFShutdownObject) {
        LOG_ERROR(Audio_DSP, "Cannot load function MFShutdownObject");
        return false;
    }

    MFCreateAlignedMemoryBuffer = Symbol<HRESULT(DWORD, DWORD, IMFMediaBuffer**)>(
        mfplat_dll.get(), "MFCreateAlignedMemoryBuffer");
    if (!MFCreateAlignedMemoryBuffer) {
        LOG_ERROR(Audio_DSP, "Cannot load function MFCreateAlignedMemoryBuffer");
        return false;
    }

    MFCreateSample = Symbol<HRESULT(IMFSample**)>(mfplat_dll.get(), "MFCreateSample");
    if (!MFCreateSample) {
        LOG_ERROR(Audio_DSP, "Cannot load function MFCreateSample");
        return false;
    }

    MFTEnumEx =
        Symbol<HRESULT(GUID, UINT32, const MFT_REGISTER_TYPE_INFO*, const MFT_REGISTER_TYPE_INFO*,
                       IMFActivate***, UINT32*)>(mfplat_dll.get(), "MFTEnumEx");
    if (!MFTEnumEx) {
        LOG_ERROR(Audio_DSP, "Cannot load function MFTEnumEx");
        return false;
    }

    MFCreateMediaType = Symbol<HRESULT(IMFMediaType**)>(mfplat_dll.get(), "MFCreateMediaType");
    if (!MFCreateMediaType) {
        LOG_ERROR(Audio_DSP, "Cannot load function MFCreateMediaType");
        return false;
    }

    return true;
}

Symbol<HRESULT(ULONG, DWORD)> MFStartup;
Symbol<HRESULT(void)> MFShutdown;
Symbol<HRESULT(IUnknown*)> MFShutdownObject;
Symbol<HRESULT(DWORD, DWORD, IMFMediaBuffer**)> MFCreateAlignedMemoryBuffer;
Symbol<HRESULT(IMFSample**)> MFCreateSample;
Symbol<HRESULT(GUID, UINT32, const MFT_REGISTER_TYPE_INFO*, const MFT_REGISTER_TYPE_INFO*,
               IMFActivate***, UINT32*)>
    MFTEnumEx;
Symbol<HRESULT(IMFMediaType**)> MFCreateMediaType;

} // namespace MFDecoder
