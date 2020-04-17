// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
}

namespace Service::AC {
class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> ac, const char* name, u32 max_session);

        /**
         * AC::CreateDefaultConfig service function
         *  Inputs:
         *      64 : ACConfig size << 14 | 2
         *      65 : pointer to ACConfig struct
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void CreateDefaultConfig(Kernel::HLERequestContext& ctx);

        /**
         * AC::ConnectAsync service function
         *  Inputs:
         *      1 : ProcessId Header
         *      3 : Copy Handle Header
         *      4 : Connection Event handle
         *      5 : ACConfig size << 14 | 2
         *      6 : pointer to ACConfig struct
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void ConnectAsync(Kernel::HLERequestContext& ctx);

        /**
         * AC::GetConnectResult service function
         *  Inputs:
         *      1 : ProcessId Header
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetConnectResult(Kernel::HLERequestContext& ctx);

        /**
         * AC::CloseAsync service function
         *  Inputs:
         *      1 : ProcessId Header
         *      3 : Copy Handle Header
         *      4 : Event handle, should be signaled when AC connection is closed
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void CloseAsync(Kernel::HLERequestContext& ctx);

        /**
         * AC::GetCloseResult service function
         *  Inputs:
         *      1 : ProcessId Header
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetCloseResult(Kernel::HLERequestContext& ctx);

        /**
         * AC::GetWifiStatus service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output connection type, 0 = none, 1 = Old3DS Internet, 2 = New3DS Internet.
         */
        void GetWifiStatus(Kernel::HLERequestContext& ctx);

        /**
         * AC::GetInfraPriority service function
         *  Inputs:
         *      1 : ACConfig size << 14 | 2
         *      2 : pointer to ACConfig struct
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Infra Priority
         */
        void GetInfraPriority(Kernel::HLERequestContext& ctx);

        /**
         * AC::SetRequestEulaVersion service function
         *  Inputs:
         *      1 : Eula Version major
         *      2 : Eula Version minor
         *      3 : ACConfig size << 14 | 2
         *      4 : Input pointer to ACConfig struct
         *      64 : ACConfig size << 14 | 2
         *      65 : Output pointer to ACConfig struct
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Infra Priority
         */
        void SetRequestEulaVersion(Kernel::HLERequestContext& ctx);

        /**
         * AC::RegisterDisconnectEvent service function
         *  Inputs:
         *      1 : ProcessId Header
         *      3 : Copy Handle Header
         *      4 : Event handle, should be signaled when AC connection is closed
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void RegisterDisconnectEvent(Kernel::HLERequestContext& ctx);

        /**
         * AC::IsConnected service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : bool, is connected
         */
        void IsConnected(Kernel::HLERequestContext& ctx);

        /**
         * AC::SetClientVersion service function
         *  Inputs:
         *      1 : Used SDK Version
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetClientVersion(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> ac;
    };

protected:
    struct ACConfig {
        std::array<u8, 0x200> data;
    };

    ACConfig default_config{};

    bool ac_connected = false;

    std::shared_ptr<Kernel::Event> close_event;
    std::shared_ptr<Kernel::Event> connect_event;
    std::shared_ptr<Kernel::Event> disconnect_event;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::AC
