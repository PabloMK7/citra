// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

extern "C" {
#include <libavcodec/avcodec.h>
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

    explicit operator bool() const {
        return ptr_function != nullptr;
    }

    T* ptr_function = nullptr;
};

extern FuncDL<int(AVSampleFormat)> av_get_bytes_per_sample_dl;
extern FuncDL<AVFrame*(void)> av_frame_alloc_dl;
extern FuncDL<void(AVFrame**)> av_frame_free_dl;
extern FuncDL<AVCodecContext*(const AVCodec*)> avcodec_alloc_context3_dl;
extern FuncDL<void(AVCodecContext**)> avcodec_free_context_dl;
extern FuncDL<int(AVCodecContext*, const AVCodec*, AVDictionary**)> avcodec_open2_dl;
extern FuncDL<AVPacket*(void)> av_packet_alloc_dl;
extern FuncDL<void(AVPacket**)> av_packet_free_dl;
extern FuncDL<AVCodec*(AVCodecID)> avcodec_find_decoder_dl;
extern FuncDL<int(AVCodecContext*, const AVPacket*)> avcodec_send_packet_dl;
extern FuncDL<int(AVCodecContext*, AVFrame*)> avcodec_receive_frame_dl;
extern FuncDL<AVCodecParserContext*(int)> av_parser_init_dl;
extern FuncDL<int(AVCodecParserContext*, AVCodecContext*, uint8_t**, int*, const uint8_t*, int,
                  int64_t, int64_t, int64_t)>
    av_parser_parse2_dl;
extern FuncDL<void(AVCodecParserContext*)> av_parser_close_dl;

bool InitFFmpegDL();

#else // _Win32

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

#endif // _Win32
