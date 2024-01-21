#include <array>
#include <optional>
#include <vector>
#include "service_fixture.h"

namespace AudioCore::HLE {
struct SourceConfiguration;
struct SourceStatus;
struct AdpcmCoefficients;
struct DspConfiguration;
struct DspStatus;
struct FinalMixSamples;
struct IntermediateMixSamples;
struct Compressor;
struct DspDebug;
} // namespace AudioCore::HLE

namespace Memory {
class MemorySystem;
}

namespace MerryAudio {

struct SharedMem {
    u16* frame_counter;

    AudioCore::HLE::SourceConfiguration* source_configurations; // access through write()
    AudioCore::HLE::SourceStatus* source_statuses;              // access through read()
    AudioCore::HLE::AdpcmCoefficients* adpcm_coefficients;      // access through write()

    AudioCore::HLE::DspConfiguration* dsp_configuration; // access through write()
    AudioCore::HLE::DspStatus* dsp_status;               // access through read()

    AudioCore::HLE::FinalMixSamples* final_samples;                   // access through read()
    AudioCore::HLE::IntermediateMixSamples* intermediate_mix_samples; // access through write()

    AudioCore::HLE::Compressor* compressor; // access through write()

    AudioCore::HLE::DspDebug* dsp_debug; // access through read()
};

struct AudioState {
    ServiceFixture::Handle pipe2_irq = ServiceFixture::PIPE2_IRQ_HANDLE;
    ServiceFixture::Handle dsp_semaphore = ServiceFixture::DSP_SEMAPHORE_HANDLE;

    std::array<std::array<u16*, 2>, 16> dsp_structs{};
    std::array<SharedMem, 2> shared_mem{};
    u16 frame_id = 4;

    const SharedMem& read() const;
    const SharedMem& write() const;
    void waitForSync();
    void notifyDsp();

    ServiceFixture* service_fixture{};
};

class MerryAudioFixture : public ServiceFixture {
public:
    std::vector<u8> loadDspFirmFromFile();
    std::optional<AudioState> audioInit(const std::vector<u8>& dspfirm);
    void audioExit(const AudioState& state);
    void initSharedMem(AudioState& state);
};
} // namespace MerryAudio
