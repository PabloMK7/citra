// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include <cubeb/cubeb.h>
#include "audio_core/audio_types.h"
#include "audio_core/cubeb_sink.h"
#include "common/logging/log.h"
#include "core/settings.h"

namespace AudioCore {

struct CubebSink::Impl {
    unsigned int sample_rate = 0;
    std::vector<std::string> device_list;

    cubeb* ctx = nullptr;
    cubeb_stream* stream = nullptr;

    std::vector<s16> queue;

    static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                             void* output_buffer, long num_frames);
    static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state);
};

CubebSink::CubebSink() : impl(std::make_unique<Impl>()) {
    if (cubeb_init(&impl->ctx, "Citra", nullptr) != CUBEB_OK) {
        NGLOG_CRITICAL(Audio_Sink, "cubeb_init failed");
        return;
    }

    const char* target_device_name = nullptr;
    cubeb_devid output_device = nullptr;

    cubeb_stream_params params;
    params.rate = native_sample_rate;
    params.channels = 2;
    params.format = CUBEB_SAMPLE_S16NE;
    params.layout = CUBEB_LAYOUT_STEREO;

    impl->sample_rate = native_sample_rate;

    u32 minimum_latency = 0;
    if (cubeb_get_min_latency(impl->ctx, &params, &minimum_latency) != CUBEB_OK)
        NGLOG_CRITICAL(Audio_Sink, "Error getting minimum latency");

    cubeb_device_collection collection;
    if (cubeb_enumerate_devices(impl->ctx, CUBEB_DEVICE_TYPE_OUTPUT, &collection) != CUBEB_OK) {
        NGLOG_WARNING(Audio_Sink, "Audio output device enumeration not supported");
    } else {
        if (collection.count >= 1 && Settings::values.audio_device_id != "auto" &&
            !Settings::values.audio_device_id.empty()) {
            target_device_name = Settings::values.audio_device_id.c_str();
        }

        for (size_t i = 0; i < collection.count; i++) {
            const cubeb_device_info& device = collection.device[i];
            if (device.friendly_name) {
                impl->device_list.emplace_back(device.friendly_name);

                if (target_device_name && strcmp(target_device_name, device.friendly_name) == 0) {
                    output_device = device.devid;
                }
            }
        }

        cubeb_device_collection_destroy(impl->ctx, &collection);
    }

    if (cubeb_stream_init(impl->ctx, &impl->stream, "Citra Audio Output", nullptr, nullptr,
                          output_device, &params, std::max(512u, minimum_latency),
                          &Impl::DataCallback, &Impl::StateCallback, impl.get()) != CUBEB_OK) {
        NGLOG_CRITICAL(Audio_Sink, "Error initializing cubeb stream");
        return;
    }

    if (cubeb_stream_start(impl->stream) != CUBEB_OK) {
        NGLOG_CRITICAL(Audio_Sink, "Error starting cubeb stream");
        return;
    }
}

CubebSink::~CubebSink() {
    if (!impl->ctx)
        return;

    if (cubeb_stream_stop(impl->stream) != CUBEB_OK) {
        NGLOG_CRITICAL(Audio_Sink, "Error stopping cubeb stream");
    }

    cubeb_stream_destroy(impl->stream);
    cubeb_destroy(impl->ctx);
}

unsigned int CubebSink::GetNativeSampleRate() const {
    if (!impl->ctx)
        return native_sample_rate;

    return impl->sample_rate;
}

std::vector<std::string> CubebSink::GetDeviceList() const {
    return impl->device_list;
}

void CubebSink::EnqueueSamples(const s16* samples, size_t sample_count) {
    if (!impl->ctx)
        return;

    impl->queue.reserve(impl->queue.size() + sample_count * 2);
    std::copy(samples, samples + sample_count * 2, std::back_inserter(impl->queue));
}

size_t CubebSink::SamplesInQueue() const {
    if (!impl->ctx)
        return 0;

    return impl->queue.size() / 2;
}

void CubebSink::SetDevice(int device_id) {}

long CubebSink::Impl::DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                   void* output_buffer, long num_frames) {
    Impl* impl = static_cast<Impl*>(user_data);
    u8* buffer = reinterpret_cast<u8*>(output_buffer);

    if (!impl)
        return 0;

    size_t frames_to_write = std::min(impl->queue.size() / 2, static_cast<size_t>(num_frames));

    memcpy(buffer, impl->queue.data(), frames_to_write * sizeof(s16) * 2);
    impl->queue.erase(impl->queue.begin(), impl->queue.begin() + frames_to_write * 2);

    if (frames_to_write < num_frames) {
        // Fill the rest of the frames with silence
        memset(buffer + frames_to_write * sizeof(s16) * 2, 0,
               (num_frames - frames_to_write) * sizeof(s16) * 2);
    }

    return num_frames;
}

void CubebSink::Impl::StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state) {}

} // namespace AudioCore
