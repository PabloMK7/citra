// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <vector>
#include <boost/serialization/array.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include "core/global.h"
#include "core/hle/kernel/event.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"

namespace Core {
class System;
}

namespace Service::APT {

/// Signals used by APT functions
enum class SignalType : u32 {
    None = 0x0,
    Wakeup = 0x1,
    Request = 0x2,
    Response = 0x3,
    Exit = 0x4,
    Message = 0x5,
    HomeButtonSingle = 0x6,
    HomeButtonDouble = 0x7,
    DspSleep = 0x8,
    DspWakeup = 0x9,
    WakeupByExit = 0xA,
    WakeupByPause = 0xB,
    WakeupByCancel = 0xC,
    WakeupByCancelAll = 0xD,
    WakeupByPowerButtonClick = 0xE,
    WakeupToJumpHome = 0xF,
    RequestForSysApplet = 0x10,
    WakeupToLaunchApplication = 0x11,
};

/// App Id's used by APT functions
enum class AppletId : u32 {
    None = 0,
    AnySystemApplet = 0x100,
    HomeMenu = 0x101,
    AlternateMenu = 0x103,
    Camera = 0x110,
    FriendList = 0x112,
    GameNotes = 0x113,
    InternetBrowser = 0x114,
    InstructionManual = 0x115,
    Notifications = 0x116,
    Miiverse = 0x117,
    MiiversePost = 0x118,
    AmiiboSettings = 0x119,
    AnySysLibraryApplet = 0x200,
    SoftwareKeyboard1 = 0x201,
    Ed1 = 0x202,
    PnoteApp = 0x204,
    SnoteApp = 0x205,
    Error = 0x206,
    Mint = 0x207,
    Extrapad = 0x208,
    Memolib = 0x209,
    Application = 0x300,
    Tiger = 0x301,
    AnyLibraryApplet = 0x400,
    SoftwareKeyboard2 = 0x401,
    Ed2 = 0x402,
    PnoteApp2 = 0x404,
    SnoteApp2 = 0x405,
    Error2 = 0x406,
    Mint2 = 0x407,
    Extrapad2 = 0x408,
    Memolib2 = 0x409,
};

/// Holds information about the parameters used in Send/Glance/ReceiveParameter
struct MessageParameter {
    AppletId sender_id = AppletId::None;
    AppletId destination_id = AppletId::None;
    SignalType signal = SignalType::None;
    std::shared_ptr<Kernel::Object> object = nullptr;
    std::vector<u8> buffer;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& sender_id;
        ar& destination_id;
        ar& signal;
        ar& object;
        ar& buffer;
    }
    friend class boost::serialization::access;
};

enum class AppletPos {
    Application = 0,
    Library = 1,
    System = 2,
    SysLibrary = 3,
    Resident = 4,
    AutoLibrary = 5
};

union AppletAttributes {
    u32 raw;

    BitField<0, 3, u32> applet_pos;
    BitField<29, 1, u32> is_home_menu;

    AppletAttributes() : raw(0) {}
    AppletAttributes(u32 attributes) : raw(attributes) {}
};

enum class ApplicationJumpFlags : u8 {
    UseInputParameters = 0,
    UseStoredParameters = 1,
    UseCurrentParameters = 2
};

struct DeliverArg {
    std::vector<u8> param;
    std::vector<u8> hmac;
    u64 source_program_id = std::numeric_limits<u64>::max();

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& param;
        ar& hmac;
        ar& source_program_id;
    }
    friend class boost::serialization::access;
};

struct ApplicationJumpParameters {
    u64 next_title_id;
    FS::MediaType next_media_type;
    ApplicationJumpFlags flags;

    u64 current_title_id;
    FS::MediaType current_media_type;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar& next_title_id;
        ar& next_media_type;
        if (file_version > 0) {
            ar& flags;
        }
        ar& current_title_id;
        ar& current_media_type;
    }
    friend class boost::serialization::access;
};

struct ApplicationStartParameters {
    u64 next_title_id;
    FS::MediaType next_media_type;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& next_title_id;
        ar& next_media_type;
    }
    friend class boost::serialization::access;
};

class AppletManager : public std::enable_shared_from_this<AppletManager> {
public:
    explicit AppletManager(Core::System& system);
    ~AppletManager();

    /**
     * Clears any existing parameter and places a new one. This function is currently only used by
     * HLE Applets and should be likely removed in the future
     */
    void CancelAndSendParameter(const MessageParameter& parameter);

    ResultCode SendParameter(const MessageParameter& parameter);
    ResultVal<MessageParameter> GlanceParameter(AppletId app_id);
    ResultVal<MessageParameter> ReceiveParameter(AppletId app_id);
    bool CancelParameter(bool check_sender, AppletId sender_appid, bool check_receiver,
                         AppletId receiver_appid);

    struct GetLockHandleResult {
        AppletAttributes corrected_attributes;
        u32 state;
        std::shared_ptr<Kernel::Mutex> lock;
    };
    ResultVal<GetLockHandleResult> GetLockHandle(AppletAttributes attributes);

    struct InitializeResult {
        std::shared_ptr<Kernel::Event> notification_event;
        std::shared_ptr<Kernel::Event> parameter_event;
    };
    ResultVal<InitializeResult> Initialize(AppletId app_id, AppletAttributes attributes);

    ResultCode Enable(AppletAttributes attributes);
    bool IsRegistered(AppletId app_id);

    ResultCode PrepareToStartLibraryApplet(AppletId applet_id);
    ResultCode PreloadLibraryApplet(AppletId applet_id);
    ResultCode FinishPreloadingLibraryApplet(AppletId applet_id);
    ResultCode StartLibraryApplet(AppletId applet_id, std::shared_ptr<Kernel::Object> object,
                                  const std::vector<u8>& buffer);
    ResultCode PrepareToCloseLibraryApplet(bool not_pause, bool exiting, bool jump_home);
    ResultCode CloseLibraryApplet(std::shared_ptr<Kernel::Object> object,
                                  const std::vector<u8>& buffer);
    ResultCode CancelLibraryApplet(bool app_exiting);

    ResultCode PrepareToStartSystemApplet(AppletId applet_id);
    ResultCode StartSystemApplet(AppletId applet_id, std::shared_ptr<Kernel::Object> object,
                                 const std::vector<u8>& buffer);
    ResultCode PrepareToCloseSystemApplet();
    ResultCode CloseSystemApplet(std::shared_ptr<Kernel::Object> object,
                                 const std::vector<u8>& buffer);

    ResultCode PrepareToDoApplicationJump(u64 title_id, FS::MediaType media_type,
                                          ApplicationJumpFlags flags);
    ResultCode DoApplicationJump(const DeliverArg& arg);

    boost::optional<DeliverArg> ReceiveDeliverArg() const {
        return deliver_arg;
    }
    void SetDeliverArg(boost::optional<DeliverArg> arg) {
        deliver_arg = std::move(arg);
    }

    ResultCode PrepareToStartApplication(u64 title_id, FS::MediaType media_type);
    ResultCode StartApplication(const std::vector<u8>& parameter, const std::vector<u8>& hmac,
                                bool paused);
    ResultCode WakeupApplication();

    struct AppletInfo {
        u64 title_id;
        Service::FS::MediaType media_type;
        bool registered;
        bool loaded;
        u32 attributes;
    };
    ResultVal<AppletInfo> GetAppletInfo(AppletId app_id);

    ApplicationJumpParameters GetApplicationJumpParameters() const {
        return app_jump_parameters;
    }

private:
    /// APT lock retrieved via GetLockHandle.
    std::shared_ptr<Kernel::Mutex> lock;

    /// Parameter data to be returned in the next call to Glance/ReceiveParameter.
    // NOTE: A bug in gcc prevents serializing std::optional
    boost::optional<MessageParameter> next_parameter;

    /// This parameter will be sent to the application/applet once they register themselves by using
    /// APT::Initialize.
    boost::optional<MessageParameter> delayed_parameter;

    ApplicationJumpParameters app_jump_parameters{};
    boost::optional<ApplicationStartParameters> app_start_parameters{};
    boost::optional<DeliverArg> deliver_arg{};

    static constexpr std::size_t NumAppletSlot = 4;

    enum class AppletSlot : u8 {
        Application,
        SystemApplet,
        HomeMenu,
        LibraryApplet,

        // An invalid tag
        Error,
    };

    struct AppletSlotData {
        AppletId applet_id;
        AppletSlot slot;
        u64 title_id;
        bool registered;
        bool loaded;
        AppletAttributes attributes;
        std::shared_ptr<Kernel::Event> notification_event;
        std::shared_ptr<Kernel::Event> parameter_event;

        void Reset() {
            applet_id = AppletId::None;
            registered = false;
            title_id = 0;
            attributes.raw = 0;
        }

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& applet_id;
            ar& slot;
            ar& title_id;
            ar& registered;
            ar& loaded;
            ar& attributes.raw;
            ar& notification_event;
            ar& parameter_event;
        }
        friend class boost::serialization::access;
    };

    // Holds data about the concurrently running applets in the system.
    std::array<AppletSlotData, NumAppletSlot> applet_slots = {};
    AppletSlot active_slot = AppletSlot::Error;

    AppletSlot last_library_launcher_slot = AppletSlot::Error;
    SignalType library_applet_closing_command = SignalType::None;
    AppletId last_prepared_library_applet = AppletId::None;
    AppletSlot last_system_launcher_slot = AppletSlot::Error;

    Core::System& system;

    AppletSlotData* GetAppletSlot(AppletSlot slot) {
        return &applet_slots[static_cast<std::size_t>(slot)];
    }

    AppletId GetAppletSlotId(AppletSlot slot) {
        return slot != AppletSlot::Error ? GetAppletSlot(slot)->applet_id : AppletId::None;
    }

    AppletSlot GetAppletSlotFromId(AppletId id);
    AppletSlot GetAppletSlotFromAttributes(AppletAttributes attributes);
    AppletSlot GetAppletSlotFromPos(AppletPos pos);

    /// Checks if the Application slot has already been registered and sends the parameter to it,
    /// otherwise it queues for sending when the application registers itself with APT::Enable.
    void SendApplicationParameterAfterRegistration(const MessageParameter& parameter);

    void EnsureHomeMenuLoaded();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar& next_parameter;
        ar& app_jump_parameters;
        if (file_version > 0) {
            ar& delayed_parameter;
            ar& app_start_parameters;
            ar& deliver_arg;
            ar& active_slot;
            ar& last_library_launcher_slot;
            ar& last_prepared_library_applet;
            ar& last_system_launcher_slot;
            ar& lock;
        }
        ar& applet_slots;
        ar& library_applet_closing_command;
    }
    friend class boost::serialization::access;
};

} // namespace Service::APT

BOOST_CLASS_VERSION(Service::APT::ApplicationJumpParameters, 1)
BOOST_CLASS_VERSION(Service::APT::AppletManager, 1)

SERVICE_CONSTRUCT(Service::APT::AppletManager)
