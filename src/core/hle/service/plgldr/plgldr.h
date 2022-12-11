// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright 2022 The Pixellizer Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <memory>
#include <boost/serialization/version.hpp>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PLGLDR {

class PLG_LDR final : public ServiceFramework<PLG_LDR> {
public:
    struct PluginLoaderContext {
        struct PluginLoadParameters {
            u8 no_flash = 0;
            u8 no_IR_Patch = 0;
            u32_le low_title_Id = 0;
            char path[256] = {0};
            u32_le config[32] = {0};

            template <class Archive>
            void serialize(Archive& ar, const unsigned int) {
                ar& no_flash;
                ar& no_IR_Patch;
                ar& low_title_Id;
                ar& path;
                ar& config;
            }
            friend class boost::serialization::access;
        };
        bool is_enabled = true;
        bool plugin_loaded = false;
        bool is_default_path = false;
        std::string plugin_path = "";

        bool use_user_load_parameters = false;
        PluginLoadParameters user_load_parameters;

        VAddr plg_event = 0;
        VAddr plg_reply = 0;
        Kernel::Handle memory_changed_handle = 0;

        bool is_exe_load_function_set = false;
        u32 exe_load_checksum = 0;

        std::vector<u32> load_exe_func;
        u32_le load_exe_args[4] = {0};

        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& is_enabled;
            ar& plugin_loaded;
            ar& is_default_path;
            ar& plugin_path;
            ar& use_user_load_parameters;
            ar& user_load_parameters;
            ar& plg_event;
            ar& plg_reply;
            ar& memory_changed_handle;
            ar& is_exe_load_function_set;
            ar& exe_load_checksum;
            ar& load_exe_func;
            ar& load_exe_args;
        }
        friend class boost::serialization::access;
    };

    PLG_LDR();
    ~PLG_LDR() {}

    void OnProcessRun(Kernel::Process& process, Kernel::KernelSystem& kernel);
    void OnProcessExit(Kernel::Process& process, Kernel::KernelSystem& kernel);
    ResultVal<Kernel::Handle> GetMemoryChangedHandle(Kernel::KernelSystem& kernel);
    void OnMemoryChanged(Kernel::Process& process, Kernel::KernelSystem& kernel);

    static void SetEnabled(bool enabled) {
        plgldr_context.is_enabled = enabled;
    }
    static bool GetEnabled() {
        return plgldr_context.is_enabled;
    }
    static void SetAllowGameChangeState(bool allow) {
        allow_game_change = allow;
    }
    static bool GetAllowGameChangeState() {
        return allow_game_change;
    }
    static void SetPluginFBAddr(PAddr addr) {
        plugin_fb_addr = addr;
    }
    static PAddr GetPluginFBAddr() {
        return plugin_fb_addr;
    }

private:
    static const Kernel::CoreVersion plgldr_version;

    static PluginLoaderContext plgldr_context;
    static PAddr plugin_fb_addr;
    static bool allow_game_change;

    void IsEnabled(Kernel::HLERequestContext& ctx);
    void SetEnabled(Kernel::HLERequestContext& ctx);
    void SetLoadSettings(Kernel::HLERequestContext& ctx);
    void DisplayErrorMessage(Kernel::HLERequestContext& ctx);
    void GetPLGLDRVersion(Kernel::HLERequestContext& ctx);
    void GetArbiter(Kernel::HLERequestContext& ctx);
    void GetPluginPath(Kernel::HLERequestContext& ctx);

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar& plgldr_context;
        ar& plugin_fb_addr;
        ar& allow_game_change;
    }
    friend class boost::serialization::access;
};

std::shared_ptr<PLG_LDR> GetService(Core::System& system);

void InstallInterfaces(Core::System& system);

} // namespace Service::PLGLDR

BOOST_CLASS_EXPORT_KEY(Service::PLGLDR::PLG_LDR)
