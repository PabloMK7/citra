// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include <cubeb/cubeb.h>
#include "audio_core/cubeb_input.h"
#include "common/logging/log.h"

namespace AudioCore {

struct CubebInput::Impl {
    // unsigned int sample_rate = 0;
    // std::vector<std::string> device_list;

    cubeb* ctx = nullptr;
    cubeb_stream* stream = nullptr;

    bool looped_buffer;
    u8* buffer;
    u32 buffer_size;
    u32 initial_offset;
    u32 offset;
    u32 audio_buffer_size;

    void UpdateOffset(int new_offset, Impl* impl);
    void ProcessSample(const u8* data, int current_sample_index, Impl* impl);
    static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                             void* output_buffer, long num_frames);
    static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state);
};

CubebInput::CubebInput() : impl(std::make_unique<Impl>()) {
    if (cubeb_init(&impl->ctx, "Citra Input", nullptr) != CUBEB_OK) {
        LOG_ERROR(Audio, "cubeb_init failed! Mic will not work properly");
        return;
    }
}

CubebInput::~CubebInput() {
    if (!impl->ctx)
        return;

    if (cubeb_stream_stop(impl->stream) != CUBEB_OK) {
        LOG_ERROR(Audio, "Error stopping cubeb input stream.");
    }

    cubeb_destroy(impl->ctx);
}

void CubebInput::StartSampling(Frontend::Mic::Parameters params) {
    // Cubeb apparently only supports signed 16 bit PCM (and float32 which the 3ds doesn't support)
    // TODO resample the input stream
    if (params.sign == Frontend::Mic::Signedness::Unsigned) {
        LOG_ERROR(Audio,
                  "Application requested unsupported unsigned pcm format. Falling back to signed");
    }
    if (params.sample_size != 16) {
        LOG_ERROR(Audio,
                  "Application requested unsupported 8 bit pcm format. Falling back to 16 bits");
    }

    impl->buffer = backing_memory;
    impl->buffer_size = backing_memory_size;
    impl->audio_buffer_size = params.buffer_size;
    impl->offset = params.buffer_offset;
    impl->looped_buffer = params.buffer_loop;

    cubeb_devid input_device = nullptr;
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
    }

    cubeb_stream_start(impl->stream);
}

void CubebInput::StopSampling() {
    if (impl->stream) {
        cubeb_stream_stop(impl->stream);
    }
}

void CubebInput::AdjustSampleRate(u32 sample_rate) {
    // TODO This should restart the stream with the new sample rate
    LOG_ERROR(Audio, "AdjustSampleRate unimplemented!");
}

void CubebInput::Impl::UpdateOffset(int new_offset, Impl* impl) {
    impl->offset = new_offset;
    std::memcpy(impl->buffer + impl->audio_buffer_size, reinterpret_cast<u8*>(&impl->offset),
                sizeof(u32));
}

void CubebInput::Impl::ProcessSample(const u8* data, int current_sample_index, Impl* impl) {
    if (impl->offset >= impl->audio_buffer_size) {
        if (impl->looped_buffer)
            UpdateOffset(impl->initial_offset, impl);
        else
            return;
    }

    std::memcpy(impl->buffer + impl->offset, data + current_sample_index * sizeof(u16),
                sizeof(u16));
    UpdateOffset(impl->offset + sizeof(u16), impl);
}

long CubebInput::Impl::DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                    void* output_buffer, long num_frames) {
    Impl* impl = static_cast<Impl*>(user_data);
    const u8* data = reinterpret_cast<const u8*>(input_buffer);

    if (!impl) {
        return 0;
    }

    if (!impl->buffer) {
        return 0;
    }

    for (int i = 0; i < num_frames; i++) {
        impl->ProcessSample(data, i, impl);
    }

    // returning less than num_frames here signals cubeb to stop sampling
    // return total_written;
    // TODO:: Correct this
    return num_frames;
} // namespace AudioCore

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
        for (size_t i = 0; i < collection.count; i++) {
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
