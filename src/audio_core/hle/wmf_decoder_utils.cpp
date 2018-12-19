#include "common/logging/log.h"
#include "wmf_decoder_utils.h"

// utility functions
void ReportError(std::string msg, HRESULT hr) {
    if (SUCCEEDED(hr)) {
        return;
    }
    LPSTR err;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, hr,
                  // hardcode to use en_US because if any user had problems with this
                  // we can help them w/o translating anything
                  MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPSTR)&err, 0, NULL);
    if (err != NULL) {
        LOG_CRITICAL(Audio_DSP, "{}: {}", msg, err);
    }
    LOG_CRITICAL(Audio_DSP, "{}: {:08x}", msg, hr);
}

int mf_coinit() {
    HRESULT hr = S_OK;

    // lite startup is faster and all what we need is included
    hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (hr != S_OK) {
        // Do you know you can't initialize MF in test mode or safe mode?
        ReportError("Failed to initialize Media Foundation", hr);
        return -1;
    }

    LOG_INFO(Audio_DSP, "Media Foundation activated");

    return 0;
}

int mf_decoder_init(IMFTransform** transform, GUID audio_format) {
    HRESULT hr = S_OK;
    MFT_REGISTER_TYPE_INFO reg = {0};
    GUID category = MFT_CATEGORY_AUDIO_DECODER;
    IMFActivate** activate;
    UINT32 num_activate;

    reg.guidMajorType = MFMediaType_Audio;
    reg.guidSubtype = audio_format;

    hr = MFTEnumEx(category,
                   MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER,
                   &reg, NULL, &activate, &num_activate);
    if (FAILED(hr) || num_activate < 1) {
        ReportError("Failed to enumerate decoders", hr);
        CoTaskMemFree(activate);
        return -1;
    }
    LOG_INFO(Audio_DSP, "Windows(R) Media Foundation found {} suitable decoder(s)", num_activate);
    for (unsigned int n = 0; n < num_activate; n++) {
        hr = activate[n]->ActivateObject(IID_IMFTransform, (void**)transform);
        if (FAILED(hr))
            *transform = NULL;
        activate[n]->Release();
    }
    if (*transform == NULL) {
        ReportError("Failed to initialize MFT", hr);
        CoTaskMemFree(activate);
        return -1;
    }
    CoTaskMemFree(activate);
    return 0;
}

void mf_deinit(IMFTransform** transform) {
    MFShutdownObject(*transform);
    SafeRelease(transform);
    CoUninitialize();
}

IMFSample* create_sample(void* data, DWORD len, DWORD alignment, LONGLONG duration) {
    HRESULT hr = S_OK;
    IMFMediaBuffer* buf = NULL;
    IMFSample* sample = NULL;

    hr = MFCreateSample(&sample);
    if (FAILED(hr)) {
        ReportError("Unable to allocate a sample", hr);
        return NULL;
    }
    // Yes, the argument for alignment is the actual alignment - 1
    hr = MFCreateAlignedMemoryBuffer(len, alignment - 1, &buf);
    if (FAILED(hr)) {
        ReportError("Unable to allocate a memory buffer for sample", hr);
        return NULL;
    }
    if (data) {
        BYTE* buffer;
        // lock the MediaBuffer
        // this is actually not a thread-safe lock
        hr = buf->Lock(&buffer, NULL, NULL);
        if (FAILED(hr)) {
            SafeRelease(&sample);
            SafeRelease(&buf);
            return NULL;
        }

        memcpy(buffer, data, len);

        buf->SetCurrentLength(len);
        buf->Unlock();
    }

    sample->AddBuffer(buf);
    hr = sample->SetSampleDuration(duration);
    SafeRelease(&buf);
    return sample;
}

int select_input_mediatype(IMFTransform* transform, int in_stream_id, ADTSData adts,
                           UINT8* user_data, UINT32 user_data_len, GUID audio_format) {
    HRESULT hr = S_OK;
    IMFMediaType* t;

    // actually you can get rid of the whole block of searching and filtering mess
    // if you know the exact parameters of your media stream
    hr = MFCreateMediaType(&t);
    if (FAILED(hr)) {
        ReportError("Unable to create an empty MediaType", hr);
        return -1;
    }

    // basic definition
    t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    t->SetGUID(MF_MT_SUBTYPE, audio_format);

    // see https://docs.microsoft.com/en-us/windows/desktop/medfound/aac-decoder#example-media-types
    // and https://docs.microsoft.com/zh-cn/windows/desktop/api/mmreg/ns-mmreg-heaacwaveinfo_tag
    // for the meaning of the byte array below

    // for integrate into a larger project, it is recommended to wrap the parameters into a struct
    // and pass that struct into the function
    // const UINT8 aac_data[] = { 0x01, 0x00, 0xfe, 00, 00, 00, 00, 00, 00, 00, 00, 00, 0x11, 0x90
    // }; 0: raw aac 1: adts 2: adif 3: latm/laos
    t->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1);
    t->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, adts.channels);
    t->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, adts.samplerate);
    // 0xfe = 254 = "unspecified"
    t->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 254);
    t->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
    t->SetBlob(MF_MT_USER_DATA, user_data, user_data_len);
    hr = transform->SetInputType(in_stream_id, t, 0);
    if (FAILED(hr)) {
        ReportError("failed to select input types for MFT", hr);
        return -1;
    }

    return 0;
}

int select_output_mediatype(IMFTransform* transform, int out_stream_id, GUID audio_format) {
    HRESULT hr = S_OK;
    UINT32 tmp;
    IMFMediaType* t;

    // If you know what you need and what you are doing, you can specify the condition instead of
    // searching but it's better to use search since MFT may or may not support your output
    // parameters
    for (DWORD i = 0;; i++) {
        hr = transform->GetOutputAvailableType(out_stream_id, i, &t);
        if (hr == MF_E_NO_MORE_TYPES || hr == E_NOTIMPL) {
            return 0;
        }
        if (FAILED(hr)) {
            ReportError("failed to get output types for MFT", hr);
            return -1;
        }

        hr = t->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &tmp);

        if (FAILED(hr))
            continue;
        // select PCM-16 format
        if (tmp == 32) {
            hr = t->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
            if (FAILED(hr)) {
                ReportError("failed to set MF_MT_AUDIO_BLOCK_ALIGNMENT for MFT on output stream",
                            hr);
                return -1;
            }
            hr = transform->SetOutputType(out_stream_id, t, 0);
            if (FAILED(hr)) {
                ReportError("failed to select output types for MFT", hr);
                return -1;
            }
            return 0;
        } else {
            continue;
        }

        return -1;
    }

    ReportError("MFT: Unable to find preferred output format", E_NOTIMPL);
    return -1;
}

int detect_mediatype(char* buffer, size_t len, ADTSData* output, char** aac_tag) {
    if (len < 7) {
        return -1;
    }

    ADTSData tmp;
    UINT8 aac_tmp[] = {0x01, 0x00, 0xfe, 00, 00, 00, 00, 00, 00, 00, 00, 00, 0x00, 0x00};
    uint16_t tag = 0;

    uint32_t result = parse_adts(buffer, &tmp);
    if (result == 0) {
        return -1;
    }

    tag = mf_get_aac_tag(tmp);
    aac_tmp[12] |= (tag & 0xff00) >> 8;
    aac_tmp[13] |= (tag & 0x00ff);
    memcpy(*aac_tag, aac_tmp, 14);
    memcpy(output, &tmp, sizeof(ADTSData));
    return 0;
}

int mf_flush(IMFTransform** transform) {
    HRESULT hr = (*transform)->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    if (FAILED(hr)) {
        ReportError("MFT: Flush command failed", hr);
    }
    hr = (*transform)->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
    if (FAILED(hr)) {
        ReportError("Failed to end streaming for MFT", hr);
    }

    return 0;
}

int send_sample(IMFTransform* transform, DWORD in_stream_id, IMFSample* in_sample) {
    HRESULT hr = S_OK;

    if (in_sample) {
        hr = transform->ProcessInput(in_stream_id, in_sample, 0);
        if (hr == MF_E_NOTACCEPTING) {
            return 1; // try again
        } else if (FAILED(hr)) {
            ReportError("MFT: Failed to process input", hr);
            return -1;
        } // FAILED(hr)
    } else {
        hr = transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        // ffmpeg: Some MFTs (AC3) will send a frame after each drain command (???), so
        // ffmpeg: this is required to make draining actually terminate.
        if (FAILED(hr)) {
            ReportError("MFT: Failed to drain when processing input", hr);
        }
    }

    return 0;
}

// return: 0: okay; 1: needs more sample; 2: needs reconfiguring; 3: more data available
int receive_sample(IMFTransform* transform, DWORD out_stream_id, IMFSample** out_sample) {
    HRESULT hr;
    MFT_OUTPUT_DATA_BUFFER out_buffers;
    IMFSample* sample = NULL;
    MFT_OUTPUT_STREAM_INFO out_info;
    DWORD status = 0;
    bool mft_create_sample = false;

    if (!out_sample) {
        ReportError("NULL pointer passed to receive_sample()", MF_E_SAMPLE_NOT_WRITABLE);
        return -1;
    }

    hr = transform->GetOutputStreamInfo(out_stream_id, &out_info);

    if (FAILED(hr)) {
        ReportError("MFT: Failed to get stream info", hr);
        return -1;
    }
    mft_create_sample = (out_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) ||
                        (out_info.dwFlags & MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES);

    while (true) {
        sample = NULL;
        *out_sample = NULL;
        status = 0;

        if (!mft_create_sample) {
            sample = create_sample(NULL, out_info.cbSize, out_info.cbAlignment);
            if (!sample) {
                ReportError("MFT: Unable to allocate memory for samples", hr);
                return -1;
            }
        }

        out_buffers.dwStreamID = out_stream_id;
        out_buffers.pSample = sample;

        hr = transform->ProcessOutput(0, 1, &out_buffers, &status);

        if (!FAILED(hr)) {
            *out_sample = out_buffers.pSample;
            break;
        }

        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            // TODO: better handling try again and EOF cases using drain value
            return 1;
        }

        if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
            ReportError("MFT: stream format changed, re-configuration required", hr);
            return 2;
        }

        break;
    }

    if (out_buffers.dwStatus & MFT_OUTPUT_DATA_BUFFER_INCOMPLETE) {
        return 3;
    }

    // TODO: better handling try again and EOF cases using drain value
    if (*out_sample == NULL) {
        ReportError("MFT: decoding failure", hr);
        return -1;
    }

    return 0;
}

int copy_sample_to_buffer(IMFSample* sample, void** output, DWORD* len) {
    IMFMediaBuffer* buffer;
    HRESULT hr = S_OK;
    BYTE* data;

    hr = sample->GetTotalLength(len);
    if (FAILED(hr)) {
        ReportError("Failed to get the length of sample buffer", hr);
        return -1;
    }

    sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr)) {
        ReportError("Failed to get sample buffer", hr);
        return -1;
    }

    hr = buffer->Lock(&data, NULL, NULL);
    if (FAILED(hr)) {
        ReportError("Failed to lock the buffer", hr);
        SafeRelease(&buffer);
        return -1;
    }

    *output = malloc(*len);
    memcpy(*output, data, *len);

    // if buffer unlock fails, then... whatever, we have already got data
    buffer->Unlock();
    SafeRelease(&buffer);
    return 0;
}
