// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_paths.h"
#include "core/core.h"
#include "core/hle/applets/applet.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/apt/errors.h"
#include "core/hle/service/apt/ns.h"
#include "core/hle/service/cfg/cfg.h"

SERVICE_CONSTRUCT_IMPL(Service::APT::AppletManager)

namespace Service::APT {

enum class AppletPos { Application = 0, Library = 1, System = 2, SysLibrary = 3, Resident = 4 };

struct AppletTitleData {
    // There are two possible applet ids for each applet.
    std::array<AppletId, 2> applet_ids;

    // There's a specific TitleId per region for each applet.
    static constexpr std::size_t NumRegions = 7;
    std::array<u64, NumRegions> title_ids;
};

static constexpr std::size_t NumApplets = 29;
static constexpr std::array<AppletTitleData, NumApplets> applet_titleids = {{
    {AppletId::HomeMenu, AppletId::None, 0x4003000008202, 0x4003000008F02, 0x4003000009802,
     0x4003000008202, 0x400300000A102, 0x400300000A902, 0x400300000B102},
    {AppletId::AlternateMenu, AppletId::None, 0x4003000008102, 0x4003000008102, 0x4003000008102,
     0x4003000008102, 0x4003000008102, 0x4003000008102, 0x4003000008102},
    {AppletId::Camera, AppletId::None, 0x4003000008402, 0x4003000009002, 0x4003000009902,
     0x4003000008402, 0x400300000A202, 0x400300000AA02, 0x400300000B202},
    {AppletId::FriendList, AppletId::None, 0x4003000008D02, 0x4003000009602, 0x4003000009F02,
     0x4003000008D02, 0x400300000A702, 0x400300000AF02, 0x400300000B702},
    {AppletId::GameNotes, AppletId::None, 0x4003000008702, 0x4003000009302, 0x4003000009C02,
     0x4003000008702, 0x400300000A502, 0x400300000AD02, 0x400300000B502},
    {AppletId::InternetBrowser, AppletId::None, 0x4003000008802, 0x4003000009402, 0x4003000009D02,
     0x4003000008802, 0x400300000A602, 0x400300000AE02, 0x400300000B602},
    {AppletId::InstructionManual, AppletId::None, 0x4003000008602, 0x4003000009202, 0x4003000009B02,
     0x4003000008602, 0x400300000A402, 0x400300000AC02, 0x400300000B402},
    {AppletId::Notifications, AppletId::None, 0x4003000008E02, 0x4003000009702, 0x400300000A002,
     0x4003000008E02, 0x400300000A802, 0x400300000B002, 0x400300000B802},
    {AppletId::Miiverse, AppletId::None, 0x400300000BC02, 0x400300000BD02, 0x400300000BE02,
     0x400300000BC02, 0x4003000009E02, 0x4003000009502, 0x400300000B902},
    // These values obtained from an older NS dump firmware 4.5
    {AppletId::MiiversePost, AppletId::None, 0x400300000BA02, 0x400300000BA02, 0x400300000BA02,
     0x400300000BA02, 0x400300000BA02, 0x400300000BA02, 0x400300000BA02},
    // {AppletId::MiiversePost, AppletId::None, 0x4003000008302, 0x4003000008B02, 0x400300000BA02,
    //  0x4003000008302, 0x0, 0x0, 0x0},
    {AppletId::AmiiboSettings, AppletId::None, 0x4003000009502, 0x4003000009E02, 0x400300000B902,
     0x4003000009502, 0x0, 0x4003000008C02, 0x400300000BF02},
    {AppletId::SoftwareKeyboard1, AppletId::SoftwareKeyboard2, 0x400300000C002, 0x400300000C802,
     0x400300000D002, 0x400300000C002, 0x400300000D802, 0x400300000DE02, 0x400300000E402},
    {AppletId::Ed1, AppletId::Ed2, 0x400300000C102, 0x400300000C902, 0x400300000D102,
     0x400300000C102, 0x400300000D902, 0x400300000DF02, 0x400300000E502},
    {AppletId::PnoteApp, AppletId::PnoteApp2, 0x400300000C302, 0x400300000CB02, 0x400300000D302,
     0x400300000C302, 0x400300000DB02, 0x400300000E102, 0x400300000E702},
    {AppletId::SnoteApp, AppletId::SnoteApp2, 0x400300000C402, 0x400300000CC02, 0x400300000D402,
     0x400300000C402, 0x400300000DC02, 0x400300000E202, 0x400300000E802},
    {AppletId::Error, AppletId::Error2, 0x400300000C502, 0x400300000C502, 0x400300000C502,
     0x400300000C502, 0x400300000CF02, 0x400300000CF02, 0x400300000CF02},
    {AppletId::Mint, AppletId::Mint2, 0x400300000C602, 0x400300000CE02, 0x400300000D602,
     0x400300000C602, 0x400300000DD02, 0x400300000E302, 0x400300000E902},
    {AppletId::Extrapad, AppletId::Extrapad2, 0x400300000CD02, 0x400300000CD02, 0x400300000CD02,
     0x400300000CD02, 0x400300000D502, 0x400300000D502, 0x400300000D502},
    {AppletId::Memolib, AppletId::Memolib2, 0x400300000F602, 0x400300000F602, 0x400300000F602,
     0x400300000F602, 0x400300000F602, 0x400300000F602, 0x400300000F602},
    // TODO(Subv): Fill in the rest of the titleids
}};

static u64 GetTitleIdForApplet(AppletId id, u32 region_value) {
    ASSERT_MSG(id != AppletId::None, "Invalid applet id");

    auto itr = std::find_if(applet_titleids.begin(), applet_titleids.end(),
                            [id](const AppletTitleData& data) {
                                return data.applet_ids[0] == id || data.applet_ids[1] == id;
                            });

    ASSERT_MSG(itr != applet_titleids.end(), "Unknown applet id 0x{:#05X}", static_cast<u32>(id));

    return itr->title_ids[region_value];
}

AppletManager::AppletSlotData* AppletManager::GetAppletSlotData(AppletId id) {
    auto GetSlot = [this](AppletSlot slot) -> AppletSlotData* {
        return &applet_slots[static_cast<std::size_t>(slot)];
    };

    if (id == AppletId::Application) {
        auto* slot = GetSlot(AppletSlot::Application);
        if (slot->applet_id != AppletId::None)
            return slot;

        return nullptr;
    }

    if (id == AppletId::AnySystemApplet) {
        auto* system_slot = GetSlot(AppletSlot::SystemApplet);
        if (system_slot->applet_id != AppletId::None)
            return system_slot;

        // The Home Menu is also a system applet, but it lives in its own slot to be able to run
        // concurrently with other system applets.
        auto* home_slot = GetSlot(AppletSlot::HomeMenu);
        if (home_slot->applet_id != AppletId::None)
            return home_slot;

        return nullptr;
    }

    if (id == AppletId::AnyLibraryApplet || id == AppletId::AnySysLibraryApplet) {
        auto* slot = GetSlot(AppletSlot::LibraryApplet);
        if (slot->applet_id == AppletId::None)
            return nullptr;

        u32 applet_pos = slot->attributes.applet_pos;

        if (id == AppletId::AnyLibraryApplet && applet_pos == static_cast<u32>(AppletPos::Library))
            return slot;

        if (id == AppletId::AnySysLibraryApplet &&
            applet_pos == static_cast<u32>(AppletPos::SysLibrary))
            return slot;

        return nullptr;
    }

    if (id == AppletId::HomeMenu || id == AppletId::AlternateMenu) {
        auto* slot = GetSlot(AppletSlot::HomeMenu);
        if (slot->applet_id != AppletId::None)
            return slot;

        return nullptr;
    }

    for (auto& slot : applet_slots) {
        if (slot.applet_id == id)
            return &slot;
    }

    return nullptr;
}

AppletManager::AppletSlotData* AppletManager::GetAppletSlotData(AppletAttributes attributes) {
    // Mapping from AppletPos to AppletSlot
    static constexpr std::array<AppletSlot, 6> applet_position_slots = {
        AppletSlot::Application,   AppletSlot::LibraryApplet, AppletSlot::SystemApplet,
        AppletSlot::LibraryApplet, AppletSlot::Error,         AppletSlot::LibraryApplet};

    u32 applet_pos = attributes.applet_pos;
    if (applet_pos >= applet_position_slots.size())
        return nullptr;

    AppletSlot slot = applet_position_slots[applet_pos];

    if (slot == AppletSlot::Error)
        return nullptr;

    // The Home Menu is a system applet, however, it has its own applet slot so that it can run
    // concurrently with other system applets.
    if (slot == AppletSlot::SystemApplet && attributes.is_home_menu)
        return &applet_slots[static_cast<std::size_t>(AppletSlot::HomeMenu)];

    return &applet_slots[static_cast<std::size_t>(slot)];
}

void AppletManager::CancelAndSendParameter(const MessageParameter& parameter) {
    next_parameter = parameter;
    // Signal the event to let the receiver know that a new parameter is ready to be read
    auto* const slot_data = GetAppletSlotData(static_cast<AppletId>(parameter.destination_id));
    if (slot_data == nullptr) {
        LOG_DEBUG(Service_APT, "No applet was registered with the id {:03X}",
                  static_cast<u32>(parameter.destination_id));
        return;
    }

    slot_data->parameter_event->Signal();
}

ResultCode AppletManager::SendParameter(const MessageParameter& parameter) {
    // A new parameter can not be sent if the previous one hasn't been consumed yet
    if (next_parameter) {
        return ResultCode(ErrCodes::ParameterPresent, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }
    CancelAndSendParameter(parameter);
    if (auto dest_applet = HLE::Applets::Applet::Get(parameter.destination_id)) {
        return dest_applet->ReceiveParameter(parameter);
    } else {
        return RESULT_SUCCESS;
    }
}

ResultVal<MessageParameter> AppletManager::GlanceParameter(AppletId app_id) {
    if (!next_parameter) {
        return ResultCode(ErrorDescription::NoData, ErrorModule::Applet, ErrorSummary::InvalidState,
                          ErrorLevel::Status);
    }

    if (next_parameter->destination_id != app_id) {
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                          ErrorLevel::Status);
    }

    MessageParameter parameter = *next_parameter;

    // Note: The NS module always clears the DSPSleep and DSPWakeup signals even in GlanceParameter.
    if (next_parameter->signal == SignalType::DspSleep ||
        next_parameter->signal == SignalType::DspWakeup) {
        next_parameter = {};
    }

    return MakeResult<MessageParameter>(std::move(parameter));
}

ResultVal<MessageParameter> AppletManager::ReceiveParameter(AppletId app_id) {
    auto result = GlanceParameter(app_id);
    if (result.Succeeded()) {
        // Clear the parameter
        next_parameter = {};
    }
    return result;
}

bool AppletManager::CancelParameter(bool check_sender, AppletId sender_appid, bool check_receiver,
                                    AppletId receiver_appid) {
    bool cancellation_success = true;

    if (!next_parameter) {
        cancellation_success = false;
    } else {
        if (check_sender && next_parameter->sender_id != sender_appid)
            cancellation_success = false;

        if (check_receiver && next_parameter->destination_id != receiver_appid)
            cancellation_success = false;
    }

    if (cancellation_success)
        next_parameter = {};

    return cancellation_success;
}

ResultVal<AppletManager::InitializeResult> AppletManager::Initialize(AppletId app_id,
                                                                     AppletAttributes attributes) {
    auto* const slot_data = GetAppletSlotData(attributes);

    // Note: The real NS service does not check if the attributes value is valid before accessing
    // the data in the array
    ASSERT_MSG(slot_data, "Invalid application attributes");

    if (slot_data->registered) {
        return ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    slot_data->applet_id = static_cast<AppletId>(app_id);
    // Note: In the real console the title id of a given applet slot is set by the APT module when
    // calling StartApplication.
    slot_data->title_id = system.Kernel().GetCurrentProcess()->codeset->program_id;
    slot_data->attributes.raw = attributes.raw;

    if (slot_data->applet_id == AppletId::Application ||
        slot_data->applet_id == AppletId::HomeMenu) {
        // Initialize the APT parameter to wake up the application.
        next_parameter.emplace();
        next_parameter->signal = SignalType::Wakeup;
        next_parameter->sender_id = AppletId::None;
        next_parameter->destination_id = app_id;
        // Not signaling the parameter event will cause the application (or Home Menu) to hang
        // during startup. In the real console, it is usually the Kernel and Home Menu who cause NS
        // to signal the HomeMenu and Application parameter events, respectively.
        slot_data->parameter_event->Signal();
    }

    return MakeResult<InitializeResult>(
        {slot_data->notification_event, slot_data->parameter_event});
}

ResultCode AppletManager::Enable(AppletAttributes attributes) {
    auto* const slot_data = GetAppletSlotData(attributes);

    if (!slot_data) {
        return ResultCode(ErrCodes::InvalidAppletSlot, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    slot_data->registered = true;

    return RESULT_SUCCESS;
}

bool AppletManager::IsRegistered(AppletId app_id) {
    const auto* slot_data = GetAppletSlotData(app_id);

    // Check if an LLE applet was registered first, then fallback to HLE applets
    bool is_registered = slot_data && slot_data->registered;

    if (!is_registered) {
        if (app_id == AppletId::AnyLibraryApplet) {
            is_registered = HLE::Applets::IsLibraryAppletRunning();
        } else if (auto applet = HLE::Applets::Applet::Get(app_id)) {
            // The applet exists, set it as registered.
            is_registered = true;
        }
    }

    return is_registered;
}

ResultCode AppletManager::PrepareToStartLibraryApplet(AppletId applet_id) {
    // The real APT service returns an error if there's a pending APT parameter when this function
    // is called.
    if (next_parameter) {
        return ResultCode(ErrCodes::ParameterPresent, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    const auto& slot = applet_slots[static_cast<std::size_t>(AppletSlot::LibraryApplet)];

    if (slot.registered) {
        return ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    auto cfg = Service::CFG::GetModule(system);
    u32 region_value = cfg->GetRegionValue();
    auto process =
        NS::LaunchTitle(FS::MediaType::NAND, GetTitleIdForApplet(applet_id, region_value));
    if (process) {
        return RESULT_SUCCESS;
    }

    // If we weren't able to load the native applet title, try to fallback to an HLE implementation.
    auto applet = HLE::Applets::Applet::Get(applet_id);
    if (applet) {
        LOG_WARNING(Service_APT, "applet has already been started id={:08X}",
                    static_cast<u32>(applet_id));
        return RESULT_SUCCESS;
    } else {
        return HLE::Applets::Applet::Create(applet_id, shared_from_this());
    }
}

ResultCode AppletManager::PreloadLibraryApplet(AppletId applet_id) {
    const auto& slot = applet_slots[static_cast<std::size_t>(AppletSlot::LibraryApplet)];

    if (slot.registered) {
        return ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    auto cfg = Service::CFG::GetModule(system);
    u32 region_value = cfg->GetRegionValue();
    auto process =
        NS::LaunchTitle(FS::MediaType::NAND, GetTitleIdForApplet(applet_id, region_value));
    if (process) {
        return RESULT_SUCCESS;
    }

    // If we weren't able to load the native applet title, try to fallback to an HLE implementation.
    auto applet = HLE::Applets::Applet::Get(applet_id);
    if (applet) {
        LOG_WARNING(Service_APT, "applet has already been started id={:08X}",
                    static_cast<u32>(applet_id));
        return RESULT_SUCCESS;
    } else {
        return HLE::Applets::Applet::Create(applet_id, shared_from_this());
    }
}

ResultCode AppletManager::FinishPreloadingLibraryApplet(AppletId applet_id) {
    // TODO(Subv): This function should fail depending on the applet preparation state.
    auto& slot = applet_slots[static_cast<std::size_t>(AppletSlot::LibraryApplet)];
    slot.loaded = true;
    return RESULT_SUCCESS;
}

ResultCode AppletManager::StartLibraryApplet(AppletId applet_id,
                                             std::shared_ptr<Kernel::Object> object,
                                             const std::vector<u8>& buffer) {
    MessageParameter param;
    param.destination_id = applet_id;
    param.sender_id = AppletId::Application;
    param.object = object;
    param.signal = SignalType::Wakeup;
    param.buffer = buffer;
    CancelAndSendParameter(param);

    // In case the applet is being HLEd, attempt to communicate with it.
    if (auto applet = HLE::Applets::Applet::Get(applet_id)) {
        AppletStartupParameter parameter;
        parameter.object = object;
        parameter.buffer = buffer;
        return applet->Start(parameter);
    } else {
        return RESULT_SUCCESS;
    }
}

ResultCode AppletManager::PrepareToCloseLibraryApplet(bool not_pause, bool exiting,
                                                      bool jump_home) {
    if (next_parameter) {
        return ResultCode(ErrCodes::ParameterPresent, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    if (!not_pause)
        library_applet_closing_command = SignalType::WakeupByPause;
    else if (jump_home)
        library_applet_closing_command = SignalType::WakeupToJumpHome;
    else if (exiting)
        library_applet_closing_command = SignalType::WakeupByCancel;
    else
        library_applet_closing_command = SignalType::WakeupByExit;

    return RESULT_SUCCESS;
}

ResultCode AppletManager::CloseLibraryApplet(std::shared_ptr<Kernel::Object> object,
                                             std::vector<u8> buffer) {
    auto& slot = applet_slots[static_cast<std::size_t>(AppletSlot::LibraryApplet)];

    MessageParameter param;
    // TODO(Subv): The destination id should be the "current applet slot id", which changes
    // constantly depending on what is going on in the system. Most of the time it is the running
    // application, but it could be something else if a system applet is launched.
    param.destination_id = AppletId::Application;
    param.sender_id = slot.applet_id;
    param.object = std::move(object);
    param.signal = library_applet_closing_command;
    param.buffer = std::move(buffer);

    ResultCode result = SendParameter(param);

    if (library_applet_closing_command != SignalType::WakeupByPause) {
        // TODO(Subv): Terminate the running applet title
        slot.Reset();
    }

    return result;
}

ResultVal<AppletManager::AppletInfo> AppletManager::GetAppletInfo(AppletId app_id) {
    const auto* slot = GetAppletSlotData(app_id);

    if (slot == nullptr || !slot->registered) {
        // See if there's an HLE applet and try to use it before erroring out.
        auto hle_applet = HLE::Applets::Applet::Get(app_id);
        if (hle_applet == nullptr) {
            return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet,
                              ErrorSummary::NotFound, ErrorLevel::Status);
        }
        LOG_WARNING(Service_APT, "Using HLE applet info for applet {:03X}",
                    static_cast<u32>(app_id));
        // TODO(Subv): Get the title id for the current applet and write it in the response[2-3]
        return MakeResult<AppletInfo>({0, Service::FS::MediaType::NAND, true, true, 0});
    }

    if (app_id == AppletId::Application) {
        // TODO(Subv): Implement this once Application launching is implemented
        LOG_ERROR(Service_APT, "Unimplemented GetAppletInfo(Application)");
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                          ErrorLevel::Status);
    }

    auto cfg = Service::CFG::GetModule(system);
    ASSERT_MSG(cfg, "CFG Module missing!");
    u32 region_value = cfg->GetRegionValue();
    return MakeResult<AppletInfo>({GetTitleIdForApplet(app_id, region_value),
                                   Service::FS::MediaType::NAND, slot->registered, slot->loaded,
                                   slot->attributes.raw});
}

ResultCode AppletManager::PrepareToDoApplicationJump(u64 title_id, FS::MediaType media_type,
                                                     ApplicationJumpFlags flags) {
    // A running application can not launch another application directly because the applet state
    // for the Application slot is already in use. The way this is implemented in hardware is to
    // launch the Home Menu and tell it to launch our desired application.

    // Save the title data to send it to the Home Menu when DoApplicationJump is called.
    const auto& application_slot = applet_slots[static_cast<size_t>(AppletSlot::Application)];

    ASSERT_MSG(flags != ApplicationJumpFlags::UseStoredParameters,
               "Unimplemented application jump flags 1");

    if (flags == ApplicationJumpFlags::UseCurrentParameters) {
        title_id = application_slot.title_id;
    }

    app_jump_parameters.current_title_id = application_slot.title_id;
    // TODO(Subv): Retrieve the correct media type of the currently-running application. For now
    // just assume NAND.
    app_jump_parameters.current_media_type = FS::MediaType::NAND;
    app_jump_parameters.next_title_id = title_id;
    app_jump_parameters.next_media_type = media_type;
    app_jump_parameters.flags = flags;

    // Note: The real console uses the Home Menu to perform the application jump, therefore the menu
    // needs to be running. The real APT module starts the Home Menu here if it's not already
    // running, we don't have to do this. See `EnsureHomeMenuLoaded` for launching the Home Menu.
    return RESULT_SUCCESS;
}

ResultCode AppletManager::DoApplicationJump(DeliverArg arg) {
    // Note: The real console uses the Home Menu to perform the application jump, it goes
    // OldApplication->Home Menu->NewApplication. We do not need to use the Home Menu to do this so
    // we launch the new application directly. In the real APT service, the Home Menu must be
    // running to do this, otherwise error 0xC8A0CFF0 is returned.

    auto& application_slot = applet_slots[static_cast<size_t>(AppletSlot::Application)];

    if (app_jump_parameters.flags != ApplicationJumpFlags::UseCurrentParameters) {
        // The source program ID is not updated when using flags 0x2.
        arg.source_program_id = application_slot.title_id;
    }

    application_slot.Reset();

    // Set the delivery parameters.
    deliver_arg = std::move(arg);

    // TODO(Subv): Terminate the current Application.

    // Note: The real console sends signal 17 (WakeupToLaunchApplication) to the Home Menu, this
    // prompts it to call GetProgramIdOnApplicationJump and
    // PrepareToStartApplication/StartApplication on the title to launch.

    if (app_jump_parameters.next_title_id == app_jump_parameters.current_title_id) {
        // Perform a soft-reset if we're trying to relaunch the same title.
        // TODO(Subv): Note that this reboots the entire emulated system, a better way would be to
        // simply re-launch the title without closing all services, but this would only work for
        // installed titles since we have no way of getting the file path of an arbitrary game dump
        // based only on the title id.
        system.RequestReset();
        return RESULT_SUCCESS;
    }

    // Launch the title directly.
    auto process =
        NS::LaunchTitle(app_jump_parameters.next_media_type, app_jump_parameters.next_title_id);
    if (!process) {
        LOG_CRITICAL(Service_APT, "Failed to launch title during application jump, exiting.");
        system.RequestShutdown();
    }
    return RESULT_SUCCESS;
}

void AppletManager::EnsureHomeMenuLoaded() {
    const auto& system_slot = applet_slots[static_cast<size_t>(AppletSlot::SystemApplet)];
    // TODO(Subv): The real APT service sends signal 12 (WakeupByCancel) to the currently running
    // System applet, waits for it to finish, and then launches the Home Menu.
    ASSERT_MSG(!system_slot.registered, "A system applet is already running");

    const auto& menu_slot = applet_slots[static_cast<size_t>(AppletSlot::HomeMenu)];

    if (menu_slot.registered) {
        // The Home Menu is already running.
        return;
    }

    auto cfg = Service::CFG::GetModule(system);
    ASSERT_MSG(cfg, "CFG Module missing!");
    u32 region_value = cfg->GetRegionValue();
    u64 menu_title_id = GetTitleIdForApplet(AppletId::HomeMenu, region_value);
    auto process = NS::LaunchTitle(FS::MediaType::NAND, menu_title_id);
    if (!process) {
        LOG_WARNING(Service_APT,
                    "The Home Menu failed to launch, application jumping will not work.");
    }
}

AppletManager::AppletManager(Core::System& system) : system(system) {
    for (std::size_t slot = 0; slot < applet_slots.size(); ++slot) {
        auto& slot_data = applet_slots[slot];
        slot_data.slot = static_cast<AppletSlot>(slot);
        slot_data.applet_id = AppletId::None;
        slot_data.attributes.raw = 0;
        slot_data.registered = false;
        slot_data.loaded = false;
        slot_data.notification_event =
            system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "APT:Notification");
        slot_data.parameter_event =
            system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "APT:Parameter");
    }
    HLE::Applets::Init();
}

AppletManager::~AppletManager() {
    HLE::Applets::Shutdown();
}

} // namespace Service::APT
