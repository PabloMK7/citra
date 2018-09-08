// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <vector>
#include <cubeb/cubeb.h>
#include "audio_core/audio_types.h"
#include "audio_core/cubeb_sink.h"
#include "common/logging/log.h"

namespace AudioCore {

struct CubebSink::Impl {
    unsigned int sample_rate = 0;

    cubeb* ctx = nullptr;
    cubeb_stream* stream = nullptr;

    std::function<void(s16*, std::size_t)> cb;

    static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                             void* output_buffer, long num_frames);
    static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state);
};

CubebSink::CubebSink(std::string target_device_name) : impl(std::make_unique<Impl>()) {
    if (cubeb_init(&impl->ctx, "Citra", nullptr) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "cubeb_init failed");
        return;
    }

    cubeb_devid output_device = nullptr;

    cubeb_stream_params params;
    params.rate = native_sample_rate;
    params.channels = 2;
    params.format = CUBEB_SAMPLE_S16NE;
    params.layout = CUBEB_LAYOUT_STEREO;

    impl->sample_rate = native_sample_rate;

    u32 minimum_latency = 0;
    if (cubeb_get_min_latency(impl->ctx, &params, &minimum_latency) != CUBEB_OK)
        LOG_CRITICAL(Audio_Sink, "Error getting minimum latency");

    if (target_device_name != auto_device_name && !target_device_name.empty()) {
        cubeb_device_collection collection;
        if (cubeb_enumerate_devices(impl->ctx, CUBEB_DEVICE_TYPE_OUTPUT, &collection) != CUBEB_OK) {
            LOG_WARNING(Audio_Sink, "Audio output device enumeration not supported");
        } else {
            const auto collection_end = collection.device + collection.count;
            const auto device = std::find_if(collection.device, collection_end,
                                             [&](const cubeb_device_info& device) {
                                                 return target_device_name == device.friendly_name;
                                             });
            if (device != collection_end) {
                output_device = device->devid;
            }
            cubeb_device_collection_destroy(impl->ctx, &collection);
        }
    }

    if (cubeb_stream_init(impl->ctx, &impl->stream, "Citra Audio Output", nullptr, nullptr,
                          output_device, &params, std::max(512u, minimum_latency),
                          &Impl::DataCallback, &Impl::StateCallback, impl.get()) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "Error initializing cubeb stream");
        return;
    }

    if (cubeb_stream_start(impl->stream) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "Error starting cubeb stream");
        return;
    }
}

CubebSink::~CubebSink() {
    if (!impl->ctx)
        return;

    if (cubeb_stream_stop(impl->stream) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "Error stopping cubeb stream");
    }

    cubeb_stream_destroy(impl->stream);
    cubeb_destroy(impl->ctx);
}

unsigned int CubebSink::GetNativeSampleRate() const {
    if (!impl->ctx)
        return native_sample_rate;

    return impl->sample_rate;
}

void CubebSink::SetCallback(std::function<void(s16*, std::size_t)> cb) {
    impl->cb = cb;
}

long CubebSink::Impl::DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                   void* output_buffer, long num_frames) {
    Impl* impl = static_cast<Impl*>(user_data);
    s16* buffer = reinterpret_cast<s16*>(output_buffer);

    if (!impl || !impl->cb)
        return 0;

    impl->cb(buffer, num_frames);

    return num_frames;
}

void CubebSink::Impl::StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state) {}

std::vector<std::string> ListCubebSinkDevices() {
    std::vector<std::string> device_list;
    cubeb* ctx;

    if (cubeb_init(&ctx, "Citra Device Enumerator", nullptr) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "cubeb_init failed");
        return {};
    }

    cubeb_device_collection collection;
    if (cubeb_enumerate_devices(ctx, CUBEB_DEVICE_TYPE_OUTPUT, &collection) != CUBEB_OK) {
        LOG_WARNING(Audio_Sink, "Audio output device enumeration not supported");
    } else {
        for (std::size_t i = 0; i < collection.count; i++) {
            const cubeb_device_info& device = collection.device[i];
            if (device.friendly_name) {
                device_list.emplace_back(device.friendly_name);
            }
        }
        cubeb_device_collection_destroy(ctx, &collection);
    }

    cubeb_destroy(ctx);
    return device_list;
}

} // namespace AudioCore
