// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/dumping/ffmpeg_backend.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

extern "C" {
#include <libavutil/opt.h>
}

namespace VideoDumper {

void InitializeFFmpegLibraries() {
    static bool initialized = false;

    if (initialized)
        return;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avformat_network_init();
    initialized = true;
}

FFmpegStream::~FFmpegStream() {
    Free();
}

bool FFmpegStream::Init(AVFormatContext* format_context_) {
    InitializeFFmpegLibraries();

    format_context = format_context_;
    return true;
}

void FFmpegStream::Free() {
    codec_context.reset();
}

void FFmpegStream::Flush() {
    SendFrame(nullptr);
}

void FFmpegStream::WritePacket(AVPacket& packet) {
    if (packet.pts != static_cast<s64>(AV_NOPTS_VALUE)) {
        packet.pts = av_rescale_q(packet.pts, codec_context->time_base, stream->time_base);
    }
    if (packet.dts != static_cast<s64>(AV_NOPTS_VALUE)) {
        packet.dts = av_rescale_q(packet.dts, codec_context->time_base, stream->time_base);
    }
    packet.stream_index = stream->index;
    av_interleaved_write_frame(format_context, &packet);
}

void FFmpegStream::SendFrame(AVFrame* frame) {
    // Initialize packet
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = nullptr;
    packet.size = 0;

    // Encode frame
    if (avcodec_send_frame(codec_context.get(), frame) < 0) {
        LOG_ERROR(Render, "Frame dropped: could not send frame");
        return;
    }
    int error = 1;
    while (error >= 0) {
        error = avcodec_receive_packet(codec_context.get(), &packet);
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF)
            return;
        if (error < 0) {
            LOG_ERROR(Render, "Frame dropped: could not encode audio");
            return;
        } else {
            // Write frame to video file
            WritePacket(packet);
        }
    }
}

FFmpegVideoStream::~FFmpegVideoStream() {
    Free();
}

bool FFmpegVideoStream::Init(AVFormatContext* format_context, AVOutputFormat* output_format,
                             const Layout::FramebufferLayout& layout_) {

    InitializeFFmpegLibraries();

    if (!FFmpegStream::Init(format_context))
        return false;

    layout = layout_;
    frame_count = 0;

    // Initialize video codec
    // Ensure VP9 codec here, also to avoid patent issues
    constexpr AVCodecID codec_id = AV_CODEC_ID_VP9;
    const AVCodec* codec = avcodec_find_encoder(codec_id);
    codec_context.reset(avcodec_alloc_context3(codec));
    if (!codec || !codec_context) {
        LOG_ERROR(Render, "Could not find video encoder or allocate video codec context");
        return false;
    }

    // Configure video codec context
    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context->bit_rate = 2500000;
    codec_context->width = layout.width;
    codec_context->height = layout.height;
    codec_context->time_base.num = 1;
    codec_context->time_base.den = 60;
    codec_context->gop_size = 12;
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->thread_count = 8;
    if (output_format->flags & AVFMT_GLOBALHEADER)
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    av_opt_set_int(codec_context.get(), "cpu-used", 5, 0);

    if (avcodec_open2(codec_context.get(), codec, nullptr) < 0) {
        LOG_ERROR(Render, "Could not open video codec");
        return false;
    }

    // Create video stream
    stream = avformat_new_stream(format_context, codec);
    if (!stream || avcodec_parameters_from_context(stream->codecpar, codec_context.get()) < 0) {
        LOG_ERROR(Render, "Could not create video stream");
        return false;
    }

    // Allocate frames
    current_frame.reset(av_frame_alloc());
    scaled_frame.reset(av_frame_alloc());
    scaled_frame->format = codec_context->pix_fmt;
    scaled_frame->width = layout.width;
    scaled_frame->height = layout.height;
    if (av_frame_get_buffer(scaled_frame.get(), 1) < 0) {
        LOG_ERROR(Render, "Could not allocate frame buffer");
        return false;
    }

    // Create SWS Context
    auto* context = sws_getCachedContext(
        sws_context.get(), layout.width, layout.height, pixel_format, layout.width, layout.height,
        codec_context->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (context != sws_context.get())
        sws_context.reset(context);

    return true;
}

void FFmpegVideoStream::Free() {
    FFmpegStream::Free();

    current_frame.reset();
    scaled_frame.reset();
    sws_context.reset();
}

void FFmpegVideoStream::ProcessFrame(VideoFrame& frame) {
    if (frame.width != layout.width || frame.height != layout.height) {
        LOG_ERROR(Render, "Frame dropped: resolution does not match");
        return;
    }
    // Prepare frame
    current_frame->data[0] = frame.data.data();
    current_frame->linesize[0] = frame.stride;
    current_frame->format = pixel_format;
    current_frame->width = layout.width;
    current_frame->height = layout.height;

    // Scale the frame
    if (sws_context) {
        sws_scale(sws_context.get(), current_frame->data, current_frame->linesize, 0, layout.height,
                  scaled_frame->data, scaled_frame->linesize);
    }
    scaled_frame->pts = frame_count++;

    // Encode frame
    SendFrame(scaled_frame.get());
}

FFmpegAudioStream::~FFmpegAudioStream() {
    Free();
}

bool FFmpegAudioStream::Init(AVFormatContext* format_context) {
    InitializeFFmpegLibraries();

    if (!FFmpegStream::Init(format_context))
        return false;

    sample_count = 0;

    // Initialize audio codec
    constexpr AVCodecID codec_id = AV_CODEC_ID_VORBIS;
    const AVCodec* codec = avcodec_find_encoder(codec_id);
    codec_context.reset(avcodec_alloc_context3(codec));
    if (!codec || !codec_context) {
        LOG_ERROR(Render, "Could not find audio encoder or allocate audio codec context");
        return false;
    }

    // Configure audio codec context
    codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_context->bit_rate = 64000;
    codec_context->sample_fmt = codec->sample_fmts[0];
    codec_context->sample_rate = AudioCore::native_sample_rate;
    codec_context->channel_layout = AV_CH_LAYOUT_STEREO;
    codec_context->channels = 2;

    if (avcodec_open2(codec_context.get(), codec, nullptr) < 0) {
        LOG_ERROR(Render, "Could not open audio codec");
        return false;
    }

    // Create audio stream
    stream = avformat_new_stream(format_context, codec);
    if (!stream || avcodec_parameters_from_context(stream->codecpar, codec_context.get()) < 0) {

        LOG_ERROR(Render, "Could not create audio stream");
        return false;
    }

    // Allocate frame
    audio_frame.reset(av_frame_alloc());
    audio_frame->format = codec_context->sample_fmt;
    audio_frame->channel_layout = codec_context->channel_layout;
    audio_frame->channels = codec_context->channels;

    // Allocate SWR context
    auto* context =
        swr_alloc_set_opts(nullptr, codec_context->channel_layout, codec_context->sample_fmt,
                           codec_context->sample_rate, codec_context->channel_layout,
                           AV_SAMPLE_FMT_S16P, AudioCore::native_sample_rate, 0, nullptr);
    if (!context) {
        LOG_ERROR(Render, "Could not create SWR context");
        return false;
    }
    swr_context.reset(context);
    if (swr_init(swr_context.get()) < 0) {
        LOG_ERROR(Render, "Could not init SWR context");
        return false;
    }

    // Allocate resampled data
    int error =
        av_samples_alloc_array_and_samples(&resampled_data, nullptr, codec_context->channels,
                                           codec_context->frame_size, codec_context->sample_fmt, 0);
    if (error < 0) {
        LOG_ERROR(Render, "Could not allocate samples storage");
        return false;
    }

    return true;
}

void FFmpegAudioStream::Free() {
    FFmpegStream::Free();

    audio_frame.reset();
    swr_context.reset();
    // Free resampled data
    if (resampled_data) {
        av_freep(&resampled_data[0]);
    }
    av_freep(&resampled_data);
}

void FFmpegAudioStream::ProcessFrame(VariableAudioFrame& channel0, VariableAudioFrame& channel1) {
    ASSERT_MSG(channel0.size() == channel1.size(),
               "Frames of the two channels must have the same number of samples");
    std::array<const u8*, 2> src_data = {reinterpret_cast<u8*>(channel0.data()),
                                         reinterpret_cast<u8*>(channel1.data())};
    if (swr_convert(swr_context.get(), resampled_data, channel0.size(), src_data.data(),
                    channel0.size()) < 0) {

        LOG_ERROR(Render, "Audio frame dropped: Could not resample data");
        return;
    }

    // Prepare frame
    audio_frame->nb_samples = channel0.size();
    audio_frame->data[0] = resampled_data[0];
    audio_frame->data[1] = resampled_data[1];
    audio_frame->pts = sample_count;
    sample_count += channel0.size();

    SendFrame(audio_frame.get());
}

std::size_t FFmpegAudioStream::GetAudioFrameSize() const {
    ASSERT_MSG(codec_context, "Codec context is not initialized yet!");
    return codec_context->frame_size;
}

FFmpegMuxer::~FFmpegMuxer() {
    Free();
}

bool FFmpegMuxer::Init(const std::string& path, const std::string& format,
                       const Layout::FramebufferLayout& layout) {

    InitializeFFmpegLibraries();

    if (!FileUtil::CreateFullPath(path)) {
        return false;
    }

    // Get output format
    // Ensure webm here to avoid patent issues
    ASSERT_MSG(format == "webm", "Only webm is allowed for frame dumping");
    auto* output_format = av_guess_format(format.c_str(), path.c_str(), "video/webm");
    if (!output_format) {
        LOG_ERROR(Render, "Could not get format {}", format);
        return false;
    }

    // Initialize format context
    auto* format_context_raw = format_context.get();
    if (avformat_alloc_output_context2(&format_context_raw, output_format, nullptr, path.c_str()) <
        0) {

        LOG_ERROR(Render, "Could not allocate output context");
        return false;
    }
    format_context.reset(format_context_raw);

    if (!video_stream.Init(format_context.get(), output_format, layout))
        return false;
    if (!audio_stream.Init(format_context.get()))
        return false;

    // Open video file
    if (avio_open(&format_context->pb, path.c_str(), AVIO_FLAG_WRITE) < 0 ||
        avformat_write_header(format_context.get(), nullptr)) {

        LOG_ERROR(Render, "Could not open {}", path);
        return false;
    }

    LOG_INFO(Render, "Dumping frames to {} ({}x{})", path, layout.width, layout.height);
    return true;
}

void FFmpegMuxer::Free() {
    video_stream.Free();
    audio_stream.Free();
    format_context.reset();
}

void FFmpegMuxer::ProcessVideoFrame(VideoFrame& frame) {
    video_stream.ProcessFrame(frame);
}

void FFmpegMuxer::ProcessAudioFrame(VariableAudioFrame& channel0, VariableAudioFrame& channel1) {
    audio_stream.ProcessFrame(channel0, channel1);
}

void FFmpegMuxer::FlushVideo() {
    video_stream.Flush();
}

void FFmpegMuxer::FlushAudio() {
    audio_stream.Flush();
}

std::size_t FFmpegMuxer::GetAudioFrameSize() const {
    return audio_stream.GetAudioFrameSize();
}

void FFmpegMuxer::WriteTrailer() {
    av_write_trailer(format_context.get());
}

FFmpegBackend::FFmpegBackend() = default;

FFmpegBackend::~FFmpegBackend() {
    ASSERT_MSG(!IsDumping(), "Dumping must be stopped first");

    if (video_processing_thread.joinable())
        video_processing_thread.join();
    if (audio_processing_thread.joinable())
        audio_processing_thread.join();
    ffmpeg.Free();
}

bool FFmpegBackend::StartDumping(const std::string& path, const std::string& format,
                                 const Layout::FramebufferLayout& layout) {

    InitializeFFmpegLibraries();

    if (!ffmpeg.Init(path, format, layout)) {
        ffmpeg.Free();
        return false;
    }

    video_layout = layout;

    if (video_processing_thread.joinable())
        video_processing_thread.join();
    video_processing_thread = std::thread([&] {
        event1.Set();
        while (true) {
            event2.Wait();
            current_buffer = (current_buffer + 1) % 2;
            next_buffer = (current_buffer + 1) % 2;
            event1.Set();
            // Process this frame
            auto& frame = video_frame_buffers[current_buffer];
            if (frame.width == 0 && frame.height == 0) {
                // An empty frame marks the end of frame data
                ffmpeg.FlushVideo();
                break;
            }
            ffmpeg.ProcessVideoFrame(frame);
        }
        // Finish audio execution first if not done yet
        if (audio_processing_thread.joinable())
            audio_processing_thread.join();
        EndDumping();
    });

    if (audio_processing_thread.joinable())
        audio_processing_thread.join();
    audio_processing_thread = std::thread([&] {
        VariableAudioFrame channel0, channel1;
        while (true) {
            channel0 = audio_frame_queues[0].PopWait();
            channel1 = audio_frame_queues[1].PopWait();
            if (channel0.empty()) {
                // An empty frame marks the end of frame data
                ffmpeg.FlushAudio();
                break;
            }
            ffmpeg.ProcessAudioFrame(channel0, channel1);
        }
    });

    VideoCore::g_renderer->PrepareVideoDumping();
    is_dumping = true;

    return true;
}

void FFmpegBackend::AddVideoFrame(const VideoFrame& frame) {
    event1.Wait();
    video_frame_buffers[next_buffer] = std::move(frame);
    event2.Set();
}

void FFmpegBackend::AddAudioFrame(const AudioCore::StereoFrame16& frame) {
    std::array<std::array<s16, 160>, 2> refactored_frame;
    for (std::size_t i = 0; i < frame.size(); i++) {
        refactored_frame[0][i] = frame[i][0];
        refactored_frame[1][i] = frame[i][1];
    }

    for (auto i : {0, 1}) {
        audio_buffers[i].insert(audio_buffers[i].end(), refactored_frame[i].begin(),
                                refactored_frame[i].end());
    }
    CheckAudioBuffer();
}

void FFmpegBackend::AddAudioSample(const std::array<s16, 2>& sample) {
    for (auto i : {0, 1}) {
        audio_buffers[i].push_back(sample[i]);
    }
    CheckAudioBuffer();
}

void FFmpegBackend::StopDumping() {
    is_dumping = false;
    VideoCore::g_renderer->CleanupVideoDumping();

    // Flush the video processing queue
    AddVideoFrame(VideoFrame());
    for (auto i : {0, 1}) {
        // Add remaining data to audio queue
        if (audio_buffers[i].size() >= 0) {
            VariableAudioFrame buffer(audio_buffers[i].begin(), audio_buffers[i].end());
            audio_frame_queues[i].Push(std::move(buffer));
            audio_buffers[i].clear();
        }
        // Flush the audio processing queue
        audio_frame_queues[i].Push(VariableAudioFrame());
    }
    // Wait until processing ends
    processing_ended.Wait();
}

bool FFmpegBackend::IsDumping() const {
    return is_dumping.load(std::memory_order_relaxed);
}

Layout::FramebufferLayout FFmpegBackend::GetLayout() const {
    return video_layout;
}

void FFmpegBackend::EndDumping() {
    LOG_INFO(Render, "Ending frame dumping");

    ffmpeg.WriteTrailer();
    ffmpeg.Free();
    processing_ended.Set();
}

void FFmpegBackend::CheckAudioBuffer() {
    for (auto i : {0, 1}) {
        const std::size_t frame_size = ffmpeg.GetAudioFrameSize();
        // Add audio data to the queue when there is enough to form a frame
        while (audio_buffers[i].size() >= frame_size) {
            VariableAudioFrame buffer(audio_buffers[i].begin(),
                                      audio_buffers[i].begin() + frame_size);
            audio_frame_queues[i].Push(std::move(buffer));

            audio_buffers[i].erase(audio_buffers[i].begin(), audio_buffers[i].begin() + frame_size);
        }
    }
}

} // namespace VideoDumper
