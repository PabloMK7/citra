// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "common/common_types.h"
#include "common/thread.h"
#include "common/threadsafe_queue.h"
#include "core/dumping/backend.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace VideoDumper {

using VariableAudioFrame = std::vector<s16>;

void InitFFmpegLibraries();

/**
 * Wrapper around FFmpeg AVCodecContext + AVStream.
 * Rescales/Resamples, encodes and writes a frame.
 */
class FFmpegStream {
public:
    bool Init(AVFormatContext* format_context);
    void Free();
    void Flush();

protected:
    ~FFmpegStream();

    void WritePacket(AVPacket& packet);
    void SendFrame(AVFrame* frame);

    struct AVCodecContextDeleter {
        void operator()(AVCodecContext* codec_context) const {
            avcodec_free_context(&codec_context);
        }
    };

    struct AVFrameDeleter {
        void operator()(AVFrame* frame) const {
            av_frame_free(&frame);
        }
    };

    AVFormatContext* format_context{};
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codec_context{};
    AVStream* stream{};
};

/**
 * A FFmpegStream used for video data.
 * Rescales, encodes and writes a frame.
 */
class FFmpegVideoStream : public FFmpegStream {
public:
    ~FFmpegVideoStream();

    bool Init(AVFormatContext* format_context, AVOutputFormat* output_format,
              const Layout::FramebufferLayout& layout);
    void Free();
    void ProcessFrame(VideoFrame& frame);

private:
    struct SwsContextDeleter {
        void operator()(SwsContext* sws_context) const {
            sws_freeContext(sws_context);
        }
    };

    u64 frame_count{};

    std::unique_ptr<AVFrame, AVFrameDeleter> current_frame{};
    std::unique_ptr<AVFrame, AVFrameDeleter> scaled_frame{};
    std::unique_ptr<SwsContext, SwsContextDeleter> sws_context{};
    Layout::FramebufferLayout layout;

    /// The pixel format the frames are stored in
    static constexpr AVPixelFormat pixel_format = AVPixelFormat::AV_PIX_FMT_BGRA;
};

/**
 * A FFmpegStream used for audio data.
 * Resamples (converts), encodes and writes a frame.
 */
class FFmpegAudioStream : public FFmpegStream {
public:
    ~FFmpegAudioStream();

    bool Init(AVFormatContext* format_context);
    void Free();
    void ProcessFrame(VariableAudioFrame& channel0, VariableAudioFrame& channel1);
    std::size_t GetAudioFrameSize() const;

private:
    struct SwrContextDeleter {
        void operator()(SwrContext* swr_context) const {
            swr_free(&swr_context);
        }
    };

    u64 sample_count{};

    std::unique_ptr<AVFrame, AVFrameDeleter> audio_frame{};
    std::unique_ptr<SwrContext, SwrContextDeleter> swr_context{};

    u8** resampled_data{};
};

/**
 * Wrapper around FFmpeg AVFormatContext.
 * Manages the video and audio streams, and accepts video and audio data.
 */
class FFmpegMuxer {
public:
    ~FFmpegMuxer();

    bool Init(const std::string& path, const std::string& format,
              const Layout::FramebufferLayout& layout);
    void Free();
    void ProcessVideoFrame(VideoFrame& frame);
    void ProcessAudioFrame(VariableAudioFrame& channel0, VariableAudioFrame& channel1);
    void FlushVideo();
    void FlushAudio();
    std::size_t GetAudioFrameSize() const;
    void WriteTrailer();

private:
    struct AVFormatContextDeleter {
        void operator()(AVFormatContext* format_context) const {
            avio_closep(&format_context->pb);
            avformat_free_context(format_context);
        }
    };

    FFmpegAudioStream audio_stream{};
    FFmpegVideoStream video_stream{};
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> format_context{};
};

/**
 * FFmpeg video dumping backend.
 * This class implements a double buffer, and an audio queue to keep audio data
 * before enough data is received to form a frame.
 */
class FFmpegBackend : public Backend {
public:
    FFmpegBackend();
    ~FFmpegBackend() override;
    bool StartDumping(const std::string& path, const std::string& format,
                      const Layout::FramebufferLayout& layout) override;
    void AddVideoFrame(const VideoFrame& frame) override;
    void AddAudioFrame(const AudioCore::StereoFrame16& frame) override;
    void AddAudioSample(const std::array<s16, 2>& sample) override;
    void StopDumping() override;
    bool IsDumping() const override;
    Layout::FramebufferLayout GetLayout() const override;

private:
    void CheckAudioBuffer();
    void EndDumping();

    std::atomic_bool is_dumping = false; ///< Whether the backend is currently dumping

    FFmpegMuxer ffmpeg{};

    Layout::FramebufferLayout video_layout;
    std::array<VideoFrame, 2> video_frame_buffers;
    u32 current_buffer = 0, next_buffer = 1;
    Common::Event event1, event2;
    std::thread video_processing_thread;

    /// An audio buffer used to temporarily hold audio data, before the size is big enough
    /// to be sent to the encoder as a frame
    std::array<VariableAudioFrame, 2> audio_buffers;
    std::array<Common::SPSCQueue<VariableAudioFrame>, 2> audio_frame_queues;
    std::thread audio_processing_thread;

    Common::Event processing_ended;
};

} // namespace VideoDumper
