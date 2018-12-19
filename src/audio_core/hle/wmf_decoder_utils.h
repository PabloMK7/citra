#pragma once

#ifndef MF_DECODER
#define MF_DECODER

#define WINVER _WIN32_WINNT_WIN7

#include <assert.h>
#include <comdef.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mftransform.h>
#include <stdio.h>

#include <iostream>
#include <string>

#include "adts.h"

// utility functions
template <class T>
void SafeRelease(T** ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

void ReportError(std::string msg, HRESULT hr);

// exported functions
int mf_coinit();
int mf_decoder_init(IMFTransform** transform, GUID audio_format = MFAudioFormat_AAC);
void mf_deinit(IMFTransform** transform);
IMFSample* create_sample(void* data, DWORD len, DWORD alignment = 1, LONGLONG duration = 0);
int select_input_mediatype(IMFTransform* transform, int in_stream_id, ADTSData adts,
                           UINT8* user_data, UINT32 user_data_len,
                           GUID audio_format = MFAudioFormat_AAC);
int detect_mediatype(char* buffer, size_t len, ADTSData* output, char** aac_tag);
int select_output_mediatype(IMFTransform* transform, int out_stream_id,
                            GUID audio_format = MFAudioFormat_PCM);
int mf_flush(IMFTransform** transform);
int send_sample(IMFTransform* transform, DWORD in_stream_id, IMFSample* in_sample);
int receive_sample(IMFTransform* transform, DWORD out_stream_id, IMFSample** out_sample);
int copy_sample_to_buffer(IMFSample* sample, void** output, DWORD* len);

#endif // MF_DECODER
