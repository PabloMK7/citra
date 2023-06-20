// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

extern "C" {
#include <fdk-aac/aacdecoder_lib.h>
}

namespace DynamicLibrary::FdkAac {

typedef INT (*aacDecoder_GetLibInfo_func)(LIB_INFO* info);
typedef HANDLE_AACDECODER (*aacDecoder_Open_func)(TRANSPORT_TYPE transportFmt, UINT nrOfLayers);
typedef void (*aacDecoder_Close_func)(HANDLE_AACDECODER self);
typedef AAC_DECODER_ERROR (*aacDecoder_SetParam_func)(const HANDLE_AACDECODER self,
                                                      const AACDEC_PARAM param, const INT value);
typedef CStreamInfo* (*aacDecoder_GetStreamInfo_func)(HANDLE_AACDECODER self);
typedef AAC_DECODER_ERROR (*aacDecoder_DecodeFrame_func)(HANDLE_AACDECODER self, INT_PCM* pTimeData,
                                                         const INT timeDataSize, const UINT flags);
typedef AAC_DECODER_ERROR (*aacDecoder_Fill_func)(HANDLE_AACDECODER self, UCHAR* pBuffer[],
                                                  const UINT bufferSize[], UINT* bytesValid);

extern aacDecoder_GetLibInfo_func aacDecoder_GetLibInfo;
extern aacDecoder_Open_func aacDecoder_Open;
extern aacDecoder_Close_func aacDecoder_Close;
extern aacDecoder_SetParam_func aacDecoder_SetParam;
extern aacDecoder_GetStreamInfo_func aacDecoder_GetStreamInfo;
extern aacDecoder_DecodeFrame_func aacDecoder_DecodeFrame;
extern aacDecoder_Fill_func aacDecoder_Fill;

bool LoadFdkAac();

} // namespace DynamicLibrary::FdkAac
