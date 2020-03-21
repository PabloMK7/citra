// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_set>
#include "common/assert.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/string_util.h"
#include "core/dumping/ffmpeg_backend.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

extern "C" {
#include <libavutil/pixdesc.h>
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

AVDictionary* ToAVDictionary(const std::string& serialized) {
    Common::ParamPackage param_package{serialized};
    AVDictionary* result = nullptr;
    for (const auto& [key, value] : param_package) {
        av_dict_set(&result, key.c_str(), value.c_str(), 0);
    }
    return result;
}

FFmpegStream::~FFmpegStream() {
    Free();
}

bool FFmpegStream::Init(FFmpegMuxer& muxer) {
    InitializeFFmpegLibraries();

    format_context = muxer.format_context.get();
    format_context_mutex = &muxer.format_context_mutex;

    return true;
}

void FFmpegStream::Free() {
    codec_context.reset();
}

void FFmpegStream::Flush() {
    SendFrame(nullptr);
}

void FFmpegStream::WritePacket(AVPacket& packet) {
    av_packet_rescale_ts(&packet, codec_context->time_base, stream->time_base);
    packet.stream_index = stream->index;
    {
        std::lock_guard lock{*format_context_mutex};
        av_interleaved_write_frame(format_context, &packet);
    }
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

bool FFmpegVideoStream::Init(FFmpegMuxer& muxer, const Layout::FramebufferLayout& layout_) {

    InitializeFFmpegLibraries();

    if (!FFmpegStream::Init(muxer))
        return false;

    layout = layout_;
    frame_count = 0;

    // Initialize video codec
    const AVCodec* codec = avcodec_find_encoder_by_name(Settings::values.video_encoder.c_str());
    codec_context.reset(avcodec_alloc_context3(codec));
    if (!codec || !codec_context) {
        LOG_ERROR(Render, "Could not find video encoder or allocate video codec context");
        return false;
    }

    // Configure video codec context
    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context->bit_rate = Settings::values.video_bitrate;
    codec_context->width = layout.width;
    codec_context->height = layout.height;
    codec_context->time_base.num = 1;
    codec_context->time_base.den = 60;
    codec_context->gop_size = 12;
    codec_context->pix_fmt = codec->pix_fmts ? codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;
    if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    AVDictionary* options = ToAVDictionary(Settings::values.video_encoder_options);
    if (avcodec_open2(codec_context.get(), codec, &options) < 0) {
        LOG_ERROR(Render, "Could not open video codec");
        return false;
    }

    if (av_dict_count(options) != 0) { // Successfully set options are removed from the dict
        char* buf = nullptr;
        av_dict_get_string(options, &buf, ':', ';');
        LOG_WARNING(Render, "Video encoder options not found: {}", buf);
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
    if (av_frame_get_buffer(scaled_frame.get(), 0) < 0) {
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
    if (av_frame_make_writable(scaled_frame.get()) < 0) {
        LOG_ERROR(Render, "Video frame dropped: Could not prepare frame");
        return;
    }
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

bool FFmpegAudioStream::Init(FFmpegMuxer& muxer) {
    InitializeFFmpegLibraries();

    if (!FFmpegStream::Init(muxer))
        return false;

    frame_count = 0;

    // Initialize audio codec
    const AVCodec* codec = avcodec_find_encoder_by_name(Settings::values.audio_encoder.c_str());
    codec_context.reset(avcodec_alloc_context3(codec));
    if (!codec || !codec_context) {
        LOG_ERROR(Render, "Could not find audio encoder or allocate audio codec context");
        return false;
    }

    // Configure audio codec context
    codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
    codec_context->bit_rate = Settings::values.audio_bitrate;
    if (codec->sample_fmts) {
        codec_context->sample_fmt = codec->sample_fmts[0];
    } else {
        codec_context->sample_fmt = AV_SAMPLE_FMT_S16P;
    }

    if (codec->supported_samplerates) {
        codec_context->sample_rate = codec->supported_samplerates[0];
        // Prefer native sample rate if supported
        const int* ptr = codec->supported_samplerates;
        while ((*ptr)) {
            if ((*ptr) == AudioCore::native_sample_rate) {
                codec_context->sample_rate = AudioCore::native_sample_rate;
                break;
            }
            ptr++;
        }
    } else {
        codec_context->sample_rate = AudioCore::native_sample_rate;
    }
    codec_context->time_base.num = 1;
    codec_context->time_base.den = codec_context->sample_rate;
    codec_context->channel_layout = AV_CH_LAYOUT_STEREO;
    codec_context->channels = 2;
    if (format_context->oformat->flags & AVFMT_GLOBALHEADER)
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    AVDictionary* options = ToAVDictionary(Settings::values.audio_encoder_options);
    if (avcodec_open2(codec_context.get(), codec, &options) < 0) {
        LOG_ERROR(Render, "Could not open audio codec");
        return false;
    }

    if (av_dict_count(options) != 0) { // Successfully set options are removed from the dict
        char* buf = nullptr;
        av_dict_get_string(options, &buf, ':', ';');
        LOG_WARNING(Render, "Audio encoder options not found: {}", buf);
    }

    if (codec_context->frame_size) {
        frame_size = static_cast<u64>(codec_context->frame_size);
    } else { // variable frame size support
        frame_size = std::tuple_size<AudioCore::StereoFrame16>::value;
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
    audio_frame->sample_rate = codec_context->sample_rate;

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
                                           frame_size, codec_context->sample_fmt, 0);
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

void FFmpegAudioStream::ProcessFrame(const VariableAudioFrame& channel0,
                                     const VariableAudioFrame& channel1) {
    ASSERT_MSG(channel0.size() == channel1.size(),
               "Frames of the two channels must have the same number of samples");

    const auto sample_size = av_get_bytes_per_sample(codec_context->sample_fmt);
    std::array<const u8*, 2> src_data = {reinterpret_cast<const u8*>(channel0.data()),
                                         reinterpret_cast<const u8*>(channel1.data())};

    std::array<u8*, 2> dst_data;
    if (av_sample_fmt_is_planar(codec_context->sample_fmt)) {
        dst_data = {resampled_data[0] + sample_size * offset,
                    resampled_data[1] + sample_size * offset};
    } else {
        dst_data = {resampled_data[0] + sample_size * offset * 2}; // 2 channels
    }

    auto resampled_count = swr_convert(swr_context.get(), dst_data.data(), frame_size - offset,
                                       src_data.data(), channel0.size());
    if (resampled_count < 0) {
        LOG_ERROR(Render, "Audio frame dropped: Could not resample data");
        return;
    }

    offset += resampled_count;
    if (offset < frame_size) { // Still not enough to form a frame
        return;
    }

    while (true) {
        // Prepare frame
        audio_frame->nb_samples = frame_size;
        audio_frame->data[0] = resampled_data[0];
        if (av_sample_fmt_is_planar(codec_context->sample_fmt)) {
            audio_frame->data[1] = resampled_data[1];
        }
        audio_frame->pts = frame_count * frame_size;
        frame_count++;

        SendFrame(audio_frame.get());

        // swr_convert buffers input internally. Try to get more resampled data
        resampled_count = swr_convert(swr_context.get(), resampled_data, frame_size, nullptr, 0);
        if (resampled_count < 0) {
            LOG_ERROR(Render, "Audio frame dropped: Could not resample data");
            return;
        }
        if (static_cast<u64>(resampled_count) < frame_size) {
            offset = resampled_count;
            break;
        }
    }
}

void FFmpegAudioStream::Flush() {
    // Send the last samples
    audio_frame->nb_samples = offset;
    audio_frame->data[0] = resampled_data[0];
    if (av_sample_fmt_is_planar(codec_context->sample_fmt)) {
        audio_frame->data[1] = resampled_data[1];
    }
    audio_frame->pts = frame_count * frame_size;

    SendFrame(audio_frame.get());

    FFmpegStream::Flush();
}

FFmpegMuxer::~FFmpegMuxer() {
    Free();
}

bool FFmpegMuxer::Init(const std::string& path, const Layout::FramebufferLayout& layout) {

    InitializeFFmpegLibraries();

    if (!FileUtil::CreateFullPath(path)) {
        return false;
    }

    // Get output format
    const auto format = Settings::values.output_format;
    auto* output_format = av_guess_format(format.c_str(), path.c_str(), nullptr);
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

    if (!video_stream.Init(*this, layout))
        return false;
    if (!audio_stream.Init(*this))
        return false;

    AVDictionary* options = ToAVDictionary(Settings::values.format_options);
    // Open video file
    if (avio_open(&format_context->pb, path.c_str(), AVIO_FLAG_WRITE) < 0 ||
        avformat_write_header(format_context.get(), &options)) {

        LOG_ERROR(Render, "Could not open {}", path);
        return false;
    }
    if (av_dict_count(options) != 0) { // Successfully set options are removed from the dict
        char* buf = nullptr;
        av_dict_get_string(options, &buf, ':', ';');
        LOG_WARNING(Render, "Format options not found: {}", buf);
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

void FFmpegMuxer::ProcessAudioFrame(const VariableAudioFrame& channel0,
                                    const VariableAudioFrame& channel1) {
    audio_stream.ProcessFrame(channel0, channel1);
}

void FFmpegMuxer::FlushVideo() {
    video_stream.Flush();
}

void FFmpegMuxer::FlushAudio() {
    audio_stream.Flush();
}

void FFmpegMuxer::WriteTrailer() {
    std::lock_guard lock{format_context_mutex};
    av_interleaved_write_frame(format_context.get(), nullptr);
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

bool FFmpegBackend::StartDumping(const std::string& path, const Layout::FramebufferLayout& layout) {

    InitializeFFmpegLibraries();

    if (!ffmpeg.Init(path, layout)) {
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

void FFmpegBackend::AddVideoFrame(VideoFrame frame) {
    event1.Wait();
    video_frame_buffers[next_buffer] = std::move(frame);
    event2.Set();
}

void FFmpegBackend::AddAudioFrame(AudioCore::StereoFrame16 frame) {
    std::array<VariableAudioFrame, 2> refactored_frame;
    for (auto& channel : refactored_frame) {
        channel.resize(frame.size());
    }
    for (std::size_t i = 0; i < frame.size(); i++) {
        refactored_frame[0][i] = frame[i][0];
        refactored_frame[1][i] = frame[i][1];
    }

    audio_frame_queues[0].Push(std::move(refactored_frame[0]));
    audio_frame_queues[1].Push(std::move(refactored_frame[1]));
}

void FFmpegBackend::AddAudioSample(const std::array<s16, 2>& sample) {
    audio_frame_queues[0].Push(VariableAudioFrame{sample[0]});
    audio_frame_queues[1].Push(VariableAudioFrame{sample[1]});
}

void FFmpegBackend::StopDumping() {
    is_dumping = false;
    VideoCore::g_renderer->CleanupVideoDumping();

    // Flush the video processing queue
    AddVideoFrame(VideoFrame());
    for (auto i : {0, 1}) {
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

// To std string, but handles nullptr
std::string ToStdString(const char* str, const std::string& fallback = "") {
    return str ? std::string{str} : fallback;
}

std::string FormatDuration(s64 duration) {
    // The following is implemented according to libavutil code (opt.c)
    std::string out;
    if (duration < 0 && duration != std::numeric_limits<s64>::min()) {
        out.append("-");
        duration = -duration;
    }
    if (duration == std::numeric_limits<s64>::max()) {
        return "INT64_MAX";
    } else if (duration == std::numeric_limits<s64>::min()) {
        return "INT64_MIN";
    } else if (duration > 3600ll * 1000000ll) {
        out.append(fmt::format("{}:{:02d}:{:02d}.{:06d}", duration / 3600000000ll,
                               ((duration / 60000000ll) % 60), ((duration / 1000000ll) % 60),
                               duration % 1000000));
    } else if (duration > 60ll * 1000000ll) {
        out.append(fmt::format("{}:{:02d}.{:06d}", duration / 60000000ll,
                               ((duration / 1000000ll) % 60), duration % 1000000));
    } else {
        out.append(fmt::format("{}.{:06d}", duration / 1000000ll, duration % 1000000));
    }
    while (out.back() == '0') {
        out.erase(out.size() - 1, 1);
    }
    if (out.back() == '.') {
        out.erase(out.size() - 1, 1);
    }
    return out;
}

std::string FormatDefaultValue(const AVOption* option,
                               const std::vector<OptionInfo::NamedConstant>& named_constants) {
    // The following is taken and modified from libavutil code (opt.c)
    switch (option->type) {
    case AV_OPT_TYPE_BOOL: {
        const auto value = option->default_val.i64;
        if (value < 0) {
            return "auto";
        }
        return value ? "true" : "false";
    }
    case AV_OPT_TYPE_FLAGS: {
        const auto value = option->default_val.i64;
        std::string out;
        for (const auto& constant : named_constants) {
            if (!(value & constant.value)) {
                continue;
            }
            if (!out.empty()) {
                out.append("+");
            }
            out.append(constant.name);
        }
        return out.empty() ? fmt::format("{}", value) : out;
    }
    case AV_OPT_TYPE_DURATION: {
        return FormatDuration(option->default_val.i64);
    }
    case AV_OPT_TYPE_INT:
    case AV_OPT_TYPE_UINT64:
    case AV_OPT_TYPE_INT64: {
        const auto value = option->default_val.i64;
        for (const auto& constant : named_constants) {
            if (constant.value == value) {
                return constant.name;
            }
        }
        return fmt::format("{}", value);
    }
    case AV_OPT_TYPE_DOUBLE:
    case AV_OPT_TYPE_FLOAT: {
        return fmt::format("{}", option->default_val.dbl);
    }
    case AV_OPT_TYPE_RATIONAL: {
        const auto q = av_d2q(option->default_val.dbl, std::numeric_limits<int>::max());
        return fmt::format("{}/{}", q.num, q.den);
    }
    case AV_OPT_TYPE_PIXEL_FMT: {
        const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(option->default_val.i64));
        return ToStdString(name, "none");
    }
    case AV_OPT_TYPE_SAMPLE_FMT: {
        const char* name =
            av_get_sample_fmt_name(static_cast<AVSampleFormat>(option->default_val.i64));
        return ToStdString(name, "none");
    }
    case AV_OPT_TYPE_COLOR:
    case AV_OPT_TYPE_IMAGE_SIZE:
    case AV_OPT_TYPE_STRING:
    case AV_OPT_TYPE_DICT:
    case AV_OPT_TYPE_VIDEO_RATE: {
        return ToStdString(option->default_val.str);
    }
    case AV_OPT_TYPE_CHANNEL_LAYOUT: {
        return fmt::format("{:#x}", option->default_val.i64);
    }
    default:
        return "";
    }
}

void GetOptionListSingle(std::vector<OptionInfo>& out, const AVClass* av_class) {
    if (av_class == nullptr) {
        return;
    }

    const AVOption* current = nullptr;
    std::unordered_map<std::string, std::vector<OptionInfo::NamedConstant>> named_constants_map;
    // First iteration: find and place all named constants
    while ((current = av_opt_next(&av_class, current))) {
        if (current->type != AV_OPT_TYPE_CONST || !current->unit) {
            continue;
        }
        named_constants_map[current->unit].push_back(
            {current->name, ToStdString(current->help), current->default_val.i64});
    }
    // Second iteration: find all options
    current = nullptr;
    while ((current = av_opt_next(&av_class, current))) {
        // Currently we cannot handle binary options
        if (current->type == AV_OPT_TYPE_CONST || current->type == AV_OPT_TYPE_BINARY) {
            continue;
        }
        std::vector<OptionInfo::NamedConstant> named_constants;
        if (current->unit && named_constants_map.count(current->unit)) {
            named_constants = named_constants_map.at(current->unit);
        }
        const auto default_value = FormatDefaultValue(current, named_constants);
        out.push_back({current->name, ToStdString(current->help), current->type, default_value,
                       std::move(named_constants), current->min, current->max});
    }
}

void GetOptionList(std::vector<OptionInfo>& out, const AVClass* av_class, bool search_children) {
    if (av_class == nullptr) {
        return;
    }

    GetOptionListSingle(out, av_class);

    if (!search_children) {
        return;
    }

    const AVClass* child_class = nullptr;
    while ((child_class = av_opt_child_class_next(av_class, child_class))) {
        GetOptionListSingle(out, child_class);
    }
}

std::vector<OptionInfo> GetOptionList(const AVClass* av_class, bool search_children) {
    std::vector<OptionInfo> out;
    GetOptionList(out, av_class, search_children);
    return out;
}

std::vector<EncoderInfo> ListEncoders(AVMediaType type) {
    InitializeFFmpegLibraries();

    std::vector<EncoderInfo> out;

    const AVCodec* current = nullptr;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100)
    while ((current = av_codec_next(current))) {
#else
    void* data = nullptr; // For libavcodec to save the iteration state
    while ((current = av_codec_iterate(&data))) {
#endif
        if (!av_codec_is_encoder(current) || current->type != type) {
            continue;
        }
        out.push_back({current->name, ToStdString(current->long_name), current->id,
                       GetOptionList(current->priv_class, true)});
    }
    return out;
}

std::vector<OptionInfo> GetEncoderGenericOptions() {
    return GetOptionList(avcodec_get_class(), false);
}

std::vector<FormatInfo> ListFormats() {
    InitializeFFmpegLibraries();

    std::vector<FormatInfo> out;

    const AVOutputFormat* current = nullptr;
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    while ((current = av_oformat_next(current))) {
#else
    void* data = nullptr; // For libavformat to save the iteration state
    while ((current = av_muxer_iterate(&data))) {
#endif
        std::vector<std::string> extensions;
        Common::SplitString(ToStdString(current->extensions), ',', extensions);

        std::set<AVCodecID> supported_video_codecs;
        std::set<AVCodecID> supported_audio_codecs;
        // Go through all codecs
        const AVCodecDescriptor* codec = nullptr;
        while ((codec = avcodec_descriptor_next(codec))) {
            if (avformat_query_codec(current, codec->id, FF_COMPLIANCE_NORMAL) == 1) {
                if (codec->type == AVMEDIA_TYPE_VIDEO) {
                    supported_video_codecs.emplace(codec->id);
                } else if (codec->type == AVMEDIA_TYPE_AUDIO) {
                    supported_audio_codecs.emplace(codec->id);
                }
            }
        }

        if (supported_video_codecs.empty() || supported_audio_codecs.empty()) {
            continue;
        }

        out.push_back({current->name, ToStdString(current->long_name), std::move(extensions),
                       std::move(supported_video_codecs), std::move(supported_audio_codecs),
                       GetOptionList(current->priv_class, true)});
    }
    return out;
}

std::vector<OptionInfo> GetFormatGenericOptions() {
    return GetOptionList(avformat_get_class(), false);
}

} // namespace VideoDumper
