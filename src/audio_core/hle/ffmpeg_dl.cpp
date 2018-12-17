// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef _WIN32

#include <memory>
#include "audio_core/hle/ffmpeg_dl.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"

namespace {

struct LibraryDeleter {
    using pointer = HMODULE;
    void operator()(HMODULE h) const {
        if (h != nullptr)
            FreeLibrary(h);
    }
};

std::unique_ptr<HMODULE, LibraryDeleter> dll_util{nullptr};
std::unique_ptr<HMODULE, LibraryDeleter> dll_codec{nullptr};

} // namespace

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
    std::string dll_path = FileUtil::GetUserPath(FileUtil::UserPath::DLLDir);
    FileUtil::CreateDir(dll_path);
    std::wstring w_dll_path = Common::UTF8ToUTF16W(dll_path);
    SetDllDirectoryW(w_dll_path.c_str());

    dll_util.reset(LoadLibrary("avutil-56.dll"));
    if (!dll_util) {
        DWORD error_message_id = GetLastError();
        LPSTR message_buffer = nullptr;
        size_t size =
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

        std::string message(message_buffer, size);

        LocalFree(message_buffer);
        LOG_ERROR(Audio_DSP, "Could not load avutil-56.dll: {}", message);
        return false;
    }

    dll_codec.reset(LoadLibrary("avcodec-58.dll"));
    if (!dll_codec) {
        DWORD error_message_id = GetLastError();
        LPSTR message_buffer = nullptr;
        size_t size =
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

        std::string message(message_buffer, size);

        LocalFree(message_buffer);
        LOG_ERROR(Audio_DSP, "Could not load avcodec-58.dll: {}", message);
        return false;
    }
    av_get_bytes_per_sample_dl =
        FuncDL<int(AVSampleFormat)>(dll_util.get(), "av_get_bytes_per_sample");
    if (!av_get_bytes_per_sample_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_get_bytes_per_sample");
        return false;
    }

    av_frame_alloc_dl = FuncDL<AVFrame*()>(dll_util.get(), "av_frame_alloc");
    if (!av_frame_alloc_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_frame_alloc");
        return false;
    }

    av_frame_free_dl = FuncDL<void(AVFrame**)>(dll_util.get(), "av_frame_free");
    if (!av_frame_free_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_frame_free");
        return false;
    }

    avcodec_alloc_context3_dl =
        FuncDL<AVCodecContext*(const AVCodec*)>(dll_codec.get(), "avcodec_alloc_context3");
    if (!avcodec_alloc_context3_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_alloc_context3");
        return false;
    }

    avcodec_free_context_dl =
        FuncDL<void(AVCodecContext**)>(dll_codec.get(), "avcodec_free_context");
    if (!av_get_bytes_per_sample_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_free_context");
        return false;
    }

    avcodec_open2_dl = FuncDL<int(AVCodecContext*, const AVCodec*, AVDictionary**)>(
        dll_codec.get(), "avcodec_open2");
    if (!avcodec_open2_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_open2");
        return false;
    }
    av_packet_alloc_dl = FuncDL<AVPacket*(void)>(dll_codec.get(), "av_packet_alloc");
    if (!av_packet_alloc_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_packet_alloc");
        return false;
    }

    av_packet_free_dl = FuncDL<void(AVPacket**)>(dll_codec.get(), "av_packet_free");
    if (!av_packet_free_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_packet_free");
        return false;
    }

    avcodec_find_decoder_dl = FuncDL<AVCodec*(AVCodecID)>(dll_codec.get(), "avcodec_find_decoder");
    if (!avcodec_find_decoder_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_find_decoder");
        return false;
    }

    avcodec_send_packet_dl =
        FuncDL<int(AVCodecContext*, const AVPacket*)>(dll_codec.get(), "avcodec_send_packet");
    if (!avcodec_send_packet_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_send_packet");
        return false;
    }

    avcodec_receive_frame_dl =
        FuncDL<int(AVCodecContext*, AVFrame*)>(dll_codec.get(), "avcodec_receive_frame");
    if (!avcodec_receive_frame_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function avcodec_receive_frame");
        return false;
    }

    av_parser_init_dl = FuncDL<AVCodecParserContext*(int)>(dll_codec.get(), "av_parser_init");
    if (!av_parser_init_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_parser_init");
        return false;
    }

    av_parser_parse2_dl =
        FuncDL<int(AVCodecParserContext*, AVCodecContext*, uint8_t**, int*, const uint8_t*, int,
                   int64_t, int64_t, int64_t)>(dll_codec.get(), "av_parser_parse2");
    if (!av_parser_parse2_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_parser_parse2");
        return false;
    }

    av_parser_close_dl = FuncDL<void(AVCodecParserContext*)>(dll_codec.get(), "av_parser_close");
    if (!av_parser_close_dl) {
        LOG_ERROR(Audio_DSP, "Can not load function av_parser_close");
        return false;
    }

    return true;
}

#endif // _Win32
