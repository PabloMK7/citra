// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <cryptopp/osrng.h>

#include "common/assert.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#ifdef ARCHITECTURE_x86_64
#include "common/x64/cpu_detect.h"
#endif
#include "core/core.h"
#include "core/settings.h"
#include "core/telemetry_session.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/telemetry_json.h"
#include "web_service/verify_login.h"
#endif

namespace Core {

#ifdef ARCHITECTURE_x86_64
static const char* CpuVendorToStr(Common::CPUVendor vendor) {
    switch (vendor) {
    case Common::CPUVendor::INTEL:
        return "Intel";
    case Common::CPUVendor::AMD:
        return "Amd";
    case Common::CPUVendor::OTHER:
        return "Other";
    }
    UNREACHABLE();
}
#endif

static u64 GenerateTelemetryId() {
    u64 telemetry_id{};
    CryptoPP::AutoSeededRandomPool rng;
    rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&telemetry_id), sizeof(u64));
    return telemetry_id;
}

u64 GetTelemetryId() {
    u64 telemetry_id{};
    const std::string filename{FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) +
                               "telemetry_id"};

    if (FileUtil::Exists(filename)) {
        FileUtil::IOFile file(filename, "rb");
        if (!file.IsOpen()) {
            LOG_ERROR(Core, "failed to open telemetry_id: {}", filename);
            return {};
        }
        file.ReadBytes(&telemetry_id, sizeof(u64));
    } else {
        FileUtil::IOFile file(filename, "wb");
        if (!file.IsOpen()) {
            LOG_ERROR(Core, "failed to open telemetry_id: {}", filename);
            return {};
        }
        telemetry_id = GenerateTelemetryId();
        file.WriteBytes(&telemetry_id, sizeof(u64));
    }

    return telemetry_id;
}

u64 RegenerateTelemetryId() {
    const u64 new_telemetry_id{GenerateTelemetryId()};
    const std::string filename{FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) +
                               "telemetry_id"};

    FileUtil::IOFile file(filename, "wb");
    if (!file.IsOpen()) {
        LOG_ERROR(Core, "failed to open telemetry_id: {}", filename);
        return {};
    }
    file.WriteBytes(&new_telemetry_id, sizeof(u64));
    return new_telemetry_id;
}

bool VerifyLogin(const std::string& username, const std::string& token) {
#ifdef ENABLE_WEB_SERVICE
    return WebService::VerifyLogin(Settings::values.web_api_url, username, token);
#else
    return false;
#endif
}

TelemetrySession::TelemetrySession() = default;

TelemetrySession::~TelemetrySession() {
    // Log one-time session end information
    const s64 shutdown_time{std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count()};
    AddField(Telemetry::FieldType::Session, "Shutdown_Time", shutdown_time);

#ifdef ENABLE_WEB_SERVICE
    auto backend = std::make_unique<WebService::TelemetryJson>(Settings::values.web_api_url,
                                                               Settings::values.citra_username,
                                                               Settings::values.citra_token);
#else
    auto backend = std::make_unique<Telemetry::NullVisitor>();
#endif

    // Complete the session, submitting to the web service backend if necessary
    field_collection.Accept(*backend);
    if (Settings::values.enable_telemetry) {
        backend->Complete();
    }
}

void TelemetrySession::AddInitialInfo(Loader::AppLoader& app_loader) {
    // Log one-time top-level information
    AddField(Telemetry::FieldType::None, "TelemetryId", GetTelemetryId());

    // Log one-time session start information
    const s64 init_time{std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count()};
    AddField(Telemetry::FieldType::Session, "Init_Time", init_time);
    std::string program_name;
    const Loader::ResultStatus res{app_loader.ReadTitle(program_name)};
    if (res == Loader::ResultStatus::Success) {
        AddField(Telemetry::FieldType::Session, "ProgramName", program_name);
    }

    // Log application information
    const bool is_git_dirty{std::strstr(Common::g_scm_desc, "dirty") != nullptr};
    AddField(Telemetry::FieldType::App, "Git_IsDirty", is_git_dirty);
    AddField(Telemetry::FieldType::App, "Git_Branch", Common::g_scm_branch);
    AddField(Telemetry::FieldType::App, "Git_Revision", Common::g_scm_rev);
    AddField(Telemetry::FieldType::App, "BuildDate", Common::g_build_date);
    AddField(Telemetry::FieldType::App, "BuildName", Common::g_build_name);

// Log user system information
#ifdef ARCHITECTURE_x86_64
    AddField(Telemetry::FieldType::UserSystem, "CPU_Model", Common::GetCPUCaps().cpu_string);
    AddField(Telemetry::FieldType::UserSystem, "CPU_BrandString",
             Common::GetCPUCaps().brand_string);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Vendor",
             CpuVendorToStr(Common::GetCPUCaps().vendor));
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_AES", Common::GetCPUCaps().aes);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_AVX", Common::GetCPUCaps().avx);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_AVX2", Common::GetCPUCaps().avx2);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_BMI1", Common::GetCPUCaps().bmi1);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_BMI2", Common::GetCPUCaps().bmi2);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_FMA", Common::GetCPUCaps().fma);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_FMA4", Common::GetCPUCaps().fma4);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_SSE", Common::GetCPUCaps().sse);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_SSE2", Common::GetCPUCaps().sse2);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_SSE3", Common::GetCPUCaps().sse3);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_SSSE3",
             Common::GetCPUCaps().ssse3);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_SSE41",
             Common::GetCPUCaps().sse4_1);
    AddField(Telemetry::FieldType::UserSystem, "CPU_Extension_x64_SSE42",
             Common::GetCPUCaps().sse4_2);
#else
    AddField(Telemetry::FieldType::UserSystem, "CPU_Model", "Other");
#endif
#ifdef __APPLE__
    AddField(Telemetry::FieldType::UserSystem, "OsPlatform", "Apple");
#elif defined(_WIN32)
    AddField(Telemetry::FieldType::UserSystem, "OsPlatform", "Windows");
#elif defined(__linux__) || defined(linux) || defined(__linux)
    AddField(Telemetry::FieldType::UserSystem, "OsPlatform", "Linux");
#else
    AddField(Telemetry::FieldType::UserSystem, "OsPlatform", "Unknown");
#endif

    // Log user configuration information
    AddField(Telemetry::FieldType::UserConfig, "Audio_SinkId", Settings::values.sink_id);
    AddField(Telemetry::FieldType::UserConfig, "Audio_EnableAudioStretching",
             Settings::values.enable_audio_stretching);
    AddField(Telemetry::FieldType::UserConfig, "Core_UseCpuJit", Settings::values.use_cpu_jit);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_ResolutionFactor",
             Settings::values.resolution_factor);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_UseFrameLimit",
             Settings::values.use_frame_limit);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_FrameLimit", Settings::values.frame_limit);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_UseHwRenderer",
             Settings::values.use_hw_renderer);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_UseHwShader",
             Settings::values.use_hw_shader);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_ShadersAccurateMul",
             Settings::values.shaders_accurate_mul);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_UseShaderJit",
             Settings::values.use_shader_jit);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_UseVsync", Settings::values.vsync_enabled);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_FilterMode", Settings::values.filter_mode);
    AddField(Telemetry::FieldType::UserConfig, "Renderer_Render3d",
             static_cast<int>(Settings::values.render_3d));
    AddField(Telemetry::FieldType::UserConfig, "Renderer_Factor3d",
             Settings::values.factor_3d.load());
    AddField(Telemetry::FieldType::UserConfig, "System_IsNew3ds", Settings::values.is_new_3ds);
    AddField(Telemetry::FieldType::UserConfig, "System_RegionValue", Settings::values.region_value);
}

bool TelemetrySession::SubmitTestcase() {
#ifdef ENABLE_WEB_SERVICE
    auto backend = std::make_unique<WebService::TelemetryJson>(Settings::values.web_api_url,
                                                               Settings::values.citra_username,
                                                               Settings::values.citra_token);
    field_collection.Accept(*backend);
    return backend->SubmitTestcase();
#else
    return false;
#endif
}

} // namespace Core
