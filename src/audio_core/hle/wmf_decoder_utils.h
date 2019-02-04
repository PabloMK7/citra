// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#pragma once

// AAC decoder related APIs are only available with WIN7+
#define WINVER _WIN32_WINNT_WIN7

#include <string>
#include <tuple>
#include <comdef.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mftransform.h>

#include "adts.h"

enum class MFOutputState { FatalError, OK, NeedMoreInput, NeedReconfig, HaveMoreData };
enum class MFInputState { FatalError, OK, NotAccepted };

// utility functions / templates
template <class T>
struct MFRelease {
    void operator()(T* pointer) const {
        pointer->Release();
    };
};

// wrapper facilities for dealing with pointers
template <typename T>
using unique_mfptr = std::unique_ptr<T, MFRelease<T>>;

void ReportError(std::string msg, HRESULT hr);

// exported functions
bool MFCoInit();
bool MFDecoderInit(IMFTransform** transform, GUID audio_format = MFAudioFormat_AAC);
void MFDeInit(IMFTransform* transform);
unique_mfptr<IMFSample> CreateSample(void* data, DWORD len, DWORD alignment = 1,
                                     LONGLONG duration = 0);
bool SelectInputMediaType(IMFTransform* transform, int in_stream_id, const ADTSData& adts,
                          UINT8* user_data, UINT32 user_data_len,
                          GUID audio_format = MFAudioFormat_AAC);
int DetectMediaType(char* buffer, size_t len, ADTSData* output, char** aac_tag);
bool SelectOutputMediaType(IMFTransform* transform, int out_stream_id,
                           GUID audio_format = MFAudioFormat_PCM);
void MFFlush(IMFTransform* transform);
MFInputState SendSample(IMFTransform* transform, DWORD in_stream_id, IMFSample* in_sample);
std::tuple<MFOutputState, unique_mfptr<IMFSample>> ReceiveSample(IMFTransform* transform,
                                                                 DWORD out_stream_id);
int CopySampleToBuffer(IMFSample* sample, void** output, DWORD* len);
