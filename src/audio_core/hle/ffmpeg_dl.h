// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

#include "common/file_util.h"
#include "common/logging/log.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#ifdef _WIN32

template <typename T>
struct FuncDL {
    FuncDL() = default;
    FuncDL(HMODULE dll, const char* name) {
        if (dll) {
            ptr_function = reinterpret_cast<T*>(GetProcAddress(dll, name));
        }
    }

    operator T*() const {
        return ptr_function;
    }

    operator bool() const {
        return ptr_function != nullptr;
    }

    T* ptr_function = nullptr;
};

FuncDL<int(AVSampleFormat)> av_get_bytes_per_sample_dl;
FuncDL<AVFrame*(void)> av_frame_alloc_dl;
FuncDL<void(AVFrame**)> av_frame_free_dl;
FuncDL<AVCodecContext*(const AVCodec*)> avcodec_alloc_context3_dl;
FuncDL<void(AVCodecContext**)> avcodec_free_context_dl;
FuncDL<int(AVCodecContext*, const AVCodec*, AVDictionary**)> avcodec_open2_dl;
FuncDL<AVPacket*(void)> av_packet_alloc_dl;
FuncDL<void(AVPacket**)> av_packet_free_dl;
FuncDL<AVCodec*(AVCodecID)> avcodec_find_decoder_dl;
FuncDL<int(AVCodecContext*, const AVPacket*)> avcodec_send_packet_dl;
FuncDL<int(AVCodecContext*, AVFrame*)> avcodec_receive_frame_dl;
FuncDL<AVCodecParserContext*(int)> av_parser_init_dl;
FuncDL<int(AVCodecParserContext*, AVCodecContext*, uint8_t**, int*, const uint8_t*, int, int64_t,
           int64_t, int64_t)>
    av_parser_parse2_dl;
FuncDL<void(AVCodecParserContext*)> av_parser_close_dl;

bool InitFFmpegDL() {

    FileUtil::CreateDir(FileUtil::GetUserPath(FileUtil::UserPath::DLLDir));
    SetDllDirectoryA(FileUtil::GetUserPath(FileUtil::UserPath::DLLDir).c_str());

    HMODULE dll_util = nullptr;
    dll_util = LoadLibrary("avutil-56.dll");
    if (!dll_util) {
        DWORD errorMessageID = GetLastError();
        LPSTR messageBuffer = nullptr;
        size_t size =
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);

        // Free the buffer.
        LocalFree(messageBuffer);
        LOG_ERROR(Audio_DSP, "Could not load avutil-56.dll: {}", message);
        return false;
    }

    HMODULE dll_codec = nullptr;
    dll_codec = LoadLibrary("avcodec-58.dll");
    if (!dll_codec) {
        DWORD errorMessageID = GetLastError();
        LPSTR messageBuffer = nullptr;
        size_t size =
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);

        // Free the buffer.
        LocalFree(messageBuffer);
        LOG_ERROR(Audio_DSP, "Could not load avcodec-58.dll: {}", message);
        return false;
    }
    av_get_bytes_per_sample_dl = FuncDL<int(AVSampleFormat)>(dll_util, "av_get_bytes_per_sample");
    if (!av_get_bytes_per_sample_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_get_bytes_per_sample");
        return false;
    }

    av_frame_alloc_dl = FuncDL<AVFrame*()>(dll_util, "av_frame_alloc");
    if (!av_frame_alloc_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_frame_alloc");
        return false;
    }

    av_frame_free_dl = FuncDL<void(AVFrame**)>(dll_util, "av_frame_free");
    if (!av_frame_free_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_frame_free");
        return false;
    }

    avcodec_alloc_context3_dl =
        FuncDL<AVCodecContext*(const AVCodec*)>(dll_codec, "avcodec_alloc_context3");
    if (!avcodec_alloc_context3_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_alloc_context3");
        return false;
    }

    avcodec_free_context_dl = FuncDL<void(AVCodecContext**)>(dll_codec, "avcodec_free_context");
    if (!av_get_bytes_per_sample_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_free_context");
        return false;
    }

    avcodec_open2_dl =
        FuncDL<int(AVCodecContext*, const AVCodec*, AVDictionary**)>(dll_codec, "avcodec_open2");
    if (!avcodec_open2_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_open2");
        return false;
    }
    av_packet_alloc_dl = FuncDL<AVPacket*(void)>(dll_codec, "av_packet_alloc");
    if (!av_packet_alloc_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_packet_alloc");
        return false;
    }

    av_packet_free_dl = FuncDL<void(AVPacket**)>(dll_codec, "av_packet_free");
    if (!av_packet_free_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_packet_free");
        return false;
    }

    avcodec_find_decoder_dl = FuncDL<AVCodec*(AVCodecID)>(dll_codec, "avcodec_find_decoder");
    if (!avcodec_find_decoder_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_find_decoder");
        return false;
    }

    avcodec_send_packet_dl =
        FuncDL<int(AVCodecContext*, const AVPacket*)>(dll_codec, "avcodec_send_packet");
    if (!avcodec_send_packet_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_send_packet");
        return false;
    }

    avcodec_receive_frame_dl =
        FuncDL<int(AVCodecContext*, AVFrame*)>(dll_codec, "avcodec_receive_frame");
    if (!avcodec_receive_frame_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_receive_frame");
        return false;
    }

    av_parser_init_dl = FuncDL<AVCodecParserContext*(int)>(dll_codec, "av_parser_init");
    if (!av_parser_init_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_parser_init");
        return false;
    }

    av_parser_parse2_dl =
        FuncDL<int(AVCodecParserContext*, AVCodecContext*, uint8_t**, int*, const uint8_t*, int,
                   int64_t, int64_t, int64_t)>(dll_codec, "av_parser_parse2");
    if (!av_parser_parse2_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_parser_parse2");
        return false;
    }

    av_parser_close_dl = FuncDL<void(AVCodecParserContext*)>(dll_codec, "av_parser_close");
    if (!av_parser_close_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_parser_close");
        return false;
    }

    return true;
}

#endif // _Win32

#if defined(__APPLE__) || defined(__linux__)

// No dynamic loading for Unix and Apple

const auto av_get_bytes_per_sample_dl = &av_get_bytes_per_sample;
const auto av_frame_alloc_dl = &av_frame_alloc;
const auto av_frame_free_dl = &av_frame_free;
const auto avcodec_alloc_context3_dl = &avcodec_alloc_context3;
const auto avcodec_free_context_dl = &avcodec_free_context;
const auto avcodec_open2_dl = &avcodec_open2;
const auto av_packet_alloc_dl = &av_packet_alloc;
const auto av_packet_free_dl = &av_packet_free;
const auto avcodec_find_decoder_dl = &avcodec_find_decoder;
const auto avcodec_send_packet_dl = &avcodec_send_packet;
const auto avcodec_receive_frame_dl = &avcodec_receive_frame;
const auto av_parser_init_dl = &av_parser_init;
const auto av_parser_parse2_dl = &av_parser_parse2;
const auto av_parser_close_dl = &av_parser_close;

bool InitFFmpegDL() {
    return true;
}

#endif // defined(__APPLE__) || defined(__linux__)
