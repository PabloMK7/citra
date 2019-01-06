// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#pragma once

// AAC decoder related APIs are only available with WIN7+
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
        *ppT = nullptr;
    }
}

void ReportError(std::string msg, HRESULT hr);

// exported functions
int MFCoInit();
int MFDecoderInit(IMFTransform** transform, GUID audio_format = MFAudioFormat_AAC);
void MFDeInit(IMFTransform** transform);
IMFSample* CreateSample(void* data, DWORD len, DWORD alignment = 1, LONGLONG duration = 0);
bool SelectInputMediaType(IMFTransform* transform, int in_stream_id, ADTSData adts,
                           UINT8* user_data, UINT32 user_data_len,
                           GUID audio_format = MFAudioFormat_AAC);
int DetectMediaType(char* buffer, size_t len, ADTSData* output, char** aac_tag);
bool SelectOutputMediaType(IMFTransform* transform, int out_stream_id,
                            GUID audio_format = MFAudioFormat_PCM);
void MFFlush(IMFTransform** transform);
int SendSample(IMFTransform* transform, DWORD in_stream_id, IMFSample* in_sample);
int ReceiveSample(IMFTransform* transform, DWORD out_stream_id, IMFSample** out_sample);
int CopySampleToBuffer(IMFSample* sample, void** output, DWORD* len);
