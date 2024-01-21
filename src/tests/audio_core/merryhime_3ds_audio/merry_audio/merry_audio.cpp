#include <array>
#include <climits>
#include <cstdio>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "audio_core/hle/shared_memory.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "merry_audio.h"

#define VERIFY(call) call

namespace MerryAudio {
std::vector<u8> MerryAudioFixture::loadDspFirmFromFile() {
    std::string firm_filepath =
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) + "3ds" DIR_SEP "dspfirm.cdc";

    FILE* f = fopen(firm_filepath.c_str(), "rb");

    if (!f) {
        printf("Couldn't find dspfirm\n");
        return {};
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<u8> dspfirm_binary(size);
    [[maybe_unused]] std::size_t count = fread(dspfirm_binary.data(), dspfirm_binary.size(), 1, f);
    fclose(f);

    return dspfirm_binary;
}

std::optional<AudioState> MerryAudioFixture::audioInit(const std::vector<u8>& dspfirm) {
    AudioState ret;
    ret.service_fixture = this;

    if (!dspfirm.size())
        return std::nullopt;

    if (R_FAILED(dspInit())) {
        printf("dspInit() failed\n");
        return std::nullopt;
    }

    VERIFY(DSP_UnloadComponent());

    {
        bool dspfirm_loaded = false;
        VERIFY(DSP_LoadComponent(dspfirm.data(), dspfirm.size(), /*progmask=*/0xFF,
                                 /*datamask=*/0xFF, &dspfirm_loaded));
        if (!dspfirm_loaded) {
            printf("Failed to load firmware\n");
            return std::nullopt;
        }
    }

    VERIFY(svcCreateEvent(&ret.pipe2_irq, 1));

    // interrupt type == 2 (pipe related)
    // pipe channel == 2 (audio pipe)
    VERIFY(DSP_RegisterInterruptEvents(ret.pipe2_irq, 2, 2));

    VERIFY(DSP_GetSemaphoreHandle(&ret.dsp_semaphore));

    VERIFY(DSP_SetSemaphoreMask(0x2000));

    {
        // dsp_mode == 0 (request initialisation of DSP)
        const u32 dsp_mode = 0;
        VERIFY(DSP_WriteProcessPipe(2, &dsp_mode, 4));
    }

    // Inform the DSP that we have data for her.
    VERIFY(DSP_SetSemaphore(0x4000));

    // Wait for the DSP to tell us data is available.
    VERIFY(svcWaitSynchronization(ret.pipe2_irq, UINT64_MAX));
    VERIFY(svcClearEvent(ret.pipe2_irq));

    {
        u16 len_read = 0;

        u16 num_structs = 0;
        VERIFY(DSP_ReadPipeIfPossible(2, 0, &num_structs, 2, &len_read));
        if (len_read != 2) {
            printf("Reading struct addrs header: Could only read %i bytes!\n", len_read);
            return std::nullopt;
        }
        if (num_structs != 15) {
            printf("num_structs == %i (!= 15): Are you sure you have the right firmware version?\n",
                   num_structs);
            return std::nullopt;
        }

        std::array<u16, 15> dsp_addrs;
        VERIFY(DSP_ReadPipeIfPossible(2, 0, dsp_addrs.data(), 30, &len_read));
        if (len_read != 30) {
            printf("Reading struct addrs body: Could only read %i bytes!\n", len_read);
            return std::nullopt;
        }

        for (int i = 0; i < 15; i++) {
            const u32 addr0 = static_cast<u32>(dsp_addrs[i]);
            const u32 addr1 = static_cast<u32>(dsp_addrs[i]) | 0x10000;
            u16* vaddr0;
            u16* vaddr1;
            VERIFY(DSP_ConvertProcessAddressFromDspDram(addr0, &vaddr0));
            VERIFY(DSP_ConvertProcessAddressFromDspDram(addr1, &vaddr1));
            ret.dsp_structs[i][0] = reinterpret_cast<u16*>(vaddr0);
            ret.dsp_structs[i][1] = reinterpret_cast<u16*>(vaddr1);
        }

        for (int i = 0; i < 2; i++) {
            ret.shared_mem[i].frame_counter = reinterpret_cast<u16*>(ret.dsp_structs[0][i]);

            ret.shared_mem[i].source_configurations =
                reinterpret_cast<AudioCore::HLE::SourceConfiguration*>(ret.dsp_structs[1][i]);
            ret.shared_mem[i].source_statuses =
                reinterpret_cast<AudioCore::HLE::SourceStatus*>(ret.dsp_structs[2][i]);
            ret.shared_mem[i].adpcm_coefficients =
                reinterpret_cast<AudioCore::HLE::AdpcmCoefficients*>(ret.dsp_structs[3][i]);

            ret.shared_mem[i].dsp_configuration =
                reinterpret_cast<AudioCore::HLE::DspConfiguration*>(ret.dsp_structs[4][i]);
            ret.shared_mem[i].dsp_status =
                reinterpret_cast<AudioCore::HLE::DspStatus*>(ret.dsp_structs[5][i]);

            ret.shared_mem[i].final_samples =
                reinterpret_cast<AudioCore::HLE::FinalMixSamples*>(ret.dsp_structs[6][i]);
            ret.shared_mem[i].intermediate_mix_samples =
                reinterpret_cast<AudioCore::HLE::IntermediateMixSamples*>(ret.dsp_structs[7][i]);

            ret.shared_mem[i].compressor =
                reinterpret_cast<AudioCore::HLE::Compressor*>(ret.dsp_structs[8][i]);

            ret.shared_mem[i].dsp_debug =
                reinterpret_cast<AudioCore::HLE::DspDebug*>(ret.dsp_structs[9][i]);
        }
    }

    // Poke the DSP again.
    VERIFY(DSP_SetSemaphore(0x4000));

    ret.dsp_structs[0][0][0] = ret.frame_id;
    ret.frame_id++;
    VERIFY(svcSignalEvent(ret.dsp_semaphore));

    return {ret};
}

void MerryAudioFixture::audioExit(const AudioState& state) {
    {
        // dsp_mode == 1 (request shutdown of DSP)
        const u32 dsp_mode = 1;
        DSP_WriteProcessPipe(2, &dsp_mode, 4);
    }

    DSP_RegisterInterruptEvents(0, 2, 2);
    svcCloseHandle(state.pipe2_irq);
    svcCloseHandle(state.dsp_semaphore);
    DSP_UnloadComponent();

    dspExit();
}

void MerryAudioFixture::initSharedMem(AudioState& state) {
    for (auto& config : state.write().source_configurations->config) {
        {
            config.enable = 0;
            config.enable_dirty.Assign(true);
        }

        {
            config.interpolation_mode =
                AudioCore::HLE::SourceConfiguration::Configuration::InterpolationMode::None;
            // config.interpolation_related = 0;
            config.interpolation_dirty.Assign(true);
        }

        {
            config.rate_multiplier = 1.0;
            config.rate_multiplier_dirty.Assign(true);
        }

        {
            config.simple_filter_enabled.Assign(false);
            config.biquad_filter_enabled.Assign(false);
            config.filters_enabled_dirty.Assign(true);
        }

        {
            for (auto& gain : config.gain) {
                for (auto& g : gain) {
                    g = 0.0;
                }
            }
            config.gain[0][0] = 1.0;
            config.gain[0][1] = 1.0;
            config.gain_0_dirty.Assign(true);
            config.gain_1_dirty.Assign(true);
            config.gain_2_dirty.Assign(true);
        }

        {
            config.sync_count = 1;
            config.sync_count_dirty.Assign(true);
        }

        { config.reset_flag.Assign(true); }
    }

    {
        state.write().dsp_configuration->master_volume = 1.0;
        state.write().dsp_configuration->aux_return_volume[0] = 0.0;
        state.write().dsp_configuration->aux_return_volume[1] = 0.0;
        state.write().dsp_configuration->master_volume_dirty.Assign(true);
        state.write().dsp_configuration->aux_return_volume_0_dirty.Assign(true);
        state.write().dsp_configuration->aux_return_volume_1_dirty.Assign(true);
    }

    {
        state.write().dsp_configuration->output_format =
            AudioCore::HLE::DspConfiguration::OutputFormat::Stereo;
        state.write().dsp_configuration->output_format_dirty.Assign(true);
    }

    {
        state.write().dsp_configuration->clipping_mode = 0;
        state.write().dsp_configuration->clipping_mode_dirty.Assign(true);
    }

    {
        // https://www.3dbrew.org/wiki/Configuration_Memory
        state.write().dsp_configuration->headphones_connected = 0;
        state.write().dsp_configuration->headphones_connected_dirty.Assign(true);
    }
}

const SharedMem& AudioState::write() const {
    return shared_mem[frame_id % 2 == 1 ? 1 : 0];
}
const SharedMem& AudioState::read() const {
    return shared_mem[frame_id % 2 == 1 ? 1 : 0];
}

void AudioState::waitForSync() {
    if (!service_fixture) {
        return;
    }
    service_fixture->svcWaitSynchronization(pipe2_irq, UINT64_MAX);
    service_fixture->svcClearEvent(pipe2_irq);
}

void AudioState::notifyDsp() {
    write().frame_counter[0] = frame_id;
    frame_id++;
    if (service_fixture) {
        service_fixture->svcSignalEvent(dsp_semaphore);
    }
}
} // namespace MerryAudio
