// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <vector>
#include <cubeb/cubeb.h>
#include "audio_core/cubeb_input.h"
#include "common/logging/log.h"

namespace AudioCore {

using SampleQueue = Common::SPSCQueue<Frontend::Mic::Samples>;

struct CubebInput::Impl {
    cubeb* ctx = nullptr;
    cubeb_stream* stream = nullptr;

    std::unique_ptr<SampleQueue> sample_queue{};
    u8 sample_size_in_bytes = 0;

    static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                             void* output_buffer, long num_frames);
    static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state);
};

CubebInput::CubebInput(std::string device_id)
    : impl(std::make_unique<Impl>()), device_id(std::move(device_id)) {
    if (cubeb_init(&impl->ctx, "Citra Input", nullptr) != CUBEB_OK) {
        LOG_ERROR(Audio, "cubeb_init failed! Mic will not work properly");
        return;
    }
    impl->sample_queue = std::make_unique<SampleQueue>();
}

CubebInput::~CubebInput() {
    if (!impl->ctx)
        return;

    if (impl->stream) {
        if (cubeb_stream_stop(impl->stream) != CUBEB_OK) {
            LOG_ERROR(Audio, "Error stopping cubeb input stream.");
        }

        cubeb_stream_destroy(impl->stream);
    }

    cubeb_destroy(impl->ctx);
}

void CubebInput::StartSampling(const Frontend::Mic::Parameters& params) {
    // Cubeb apparently only supports signed 16 bit PCM (and float32 which the 3ds doesn't support)
    // TODO resample the input stream
    if (params.sign == Frontend::Mic::Signedness::Unsigned) {
        LOG_ERROR(Audio,
                  "Application requested unsupported unsigned pcm format. Falling back to signed");
    }

    impl->sample_size_in_bytes = params.sample_size / 8;

    parameters = params;
    is_sampling = true;

    cubeb_devid input_device = nullptr;
    if (device_id != Frontend::Mic::default_device_name && !device_id.empty()) {
        cubeb_device_collection collection;
        if (cubeb_enumerate_devices(impl->ctx, CUBEB_DEVICE_TYPE_INPUT, &collection) != CUBEB_OK) {
            LOG_WARNING(Audio, "Audio input device enumeration not supported");
        } else {
            const auto collection_end = collection.device + collection.count;
            const auto device = std::find_if(
                collection.device, collection_end, [this](const cubeb_device_info& info) {
                    return info.friendly_name != nullptr && device_id == info.friendly_name;
                });
            if (device != collection_end) {
                input_device = device->devid;
            }
            cubeb_device_collection_destroy(impl->ctx, &collection);
        }
    }

    cubeb_stream_params input_params;
    input_params.channels = 1;
    input_params.layout = CUBEB_LAYOUT_UNDEFINED;
    input_params.prefs = CUBEB_STREAM_PREF_NONE;
    input_params.format = CUBEB_SAMPLE_S16LE;
    input_params.rate = params.sample_rate;

    u32 latency_frames;
    if (cubeb_get_min_latency(impl->ctx, &input_params, &latency_frames) != CUBEB_OK) {
        LOG_ERROR(Audio, "Could not get minimum latency");
    }

    if (cubeb_stream_init(impl->ctx, &impl->stream, "Citra Microphone", input_device, &input_params,
                          nullptr, nullptr, latency_frames, Impl::DataCallback, Impl::StateCallback,
                          impl.get()) != CUBEB_OK) {
        LOG_CRITICAL(Audio, "Error creating cubeb input stream");
        is_sampling = false;
        return;
    }

    if (cubeb_stream_start(impl->stream) != CUBEB_OK) {
        LOG_CRITICAL(Audio, "Error starting cubeb input stream");
        is_sampling = false;
        return;
    }
}

void CubebInput::StopSampling() {
    // TODO(xperia64): Destroy the stream for now to avoid a leak because StartSampling
    // reinitializes the stream every time
    if (impl->stream) {
        cubeb_stream_stop(impl->stream);
        cubeb_stream_destroy(impl->stream);
        impl->stream = nullptr;
    }
    is_sampling = false;
}

void CubebInput::AdjustSampleRate(u32 sample_rate) {
    // TODO This should restart the stream with the new sample rate
    LOG_ERROR(Audio, "AdjustSampleRate unimplemented!");
}

Frontend::Mic::Samples CubebInput::Read() {
    Frontend::Mic::Samples samples{};
    Frontend::Mic::Samples queue;
    while (impl->sample_queue->Pop(queue)) {
        samples.insert(samples.end(), queue.begin(), queue.end());
    }
    return samples;
}

long CubebInput::Impl::DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                    void* output_buffer, long num_frames) {
    Impl* impl = static_cast<Impl*>(user_data);
    if (!impl) {
        return 0;
    }

    constexpr auto resample_s16_s8 = [](s16 sample) {
        return static_cast<u8>(static_cast<u16>(sample) >> 8);
    };

    std::vector<u8> samples{};
    samples.reserve(num_frames * impl->sample_size_in_bytes);
    if (impl->sample_size_in_bytes == 1) {
        // If the sample format is 8bit, then resample back to 8bit before passing back to core
        for (std::size_t i = 0; i < static_cast<std::size_t>(num_frames); i++) {
            s16 data;
            std::memcpy(&data, static_cast<const u8*>(input_buffer) + i * 2, 2);
            samples.push_back(resample_s16_s8(data));
        }
    } else {
        // Otherwise copy all of the samples to the buffer (which will be treated as s16 by core)
        const u8* data = reinterpret_cast<const u8*>(input_buffer);
        samples.insert(samples.begin(), data, data + num_frames * impl->sample_size_in_bytes);
    }
    impl->sample_queue->Push(samples);

    // returning less than num_frames here signals cubeb to stop sampling
    return num_frames;
}

void CubebInput::Impl::StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state) {}

std::vector<std::string> ListCubebInputDevices() {
    std::vector<std::string> device_list;
    cubeb* ctx;

    if (cubeb_init(&ctx, "Citra Input Device Enumerator", nullptr) != CUBEB_OK) {
        LOG_CRITICAL(Audio, "cubeb_init failed");
        return {};
    }

    cubeb_device_collection collection;
    if (cubeb_enumerate_devices(ctx, CUBEB_DEVICE_TYPE_INPUT, &collection) != CUBEB_OK) {
        LOG_WARNING(Audio_Sink, "Audio input device enumeration not supported");
    } else {
        for (std::size_t i = 0; i < collection.count; i++) {
            const cubeb_device_info& device = collection.device[i];
            if (device.state == CUBEB_DEVICE_STATE_ENABLED && device.friendly_name) {
                device_list.emplace_back(device.friendly_name);
            }
        }
        cubeb_device_collection_destroy(ctx, &collection);
    }

    cubeb_destroy(ctx);
    return device_list;
}
} // namespace AudioCore
