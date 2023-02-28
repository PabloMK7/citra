// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"
#include "core/core.h"
#include "core/hle/applets/applet.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/apt/errors.h"
#include "core/hle/service/apt/ns.h"
#include "core/hle/service/cfg/cfg.h"

SERVICE_CONSTRUCT_IMPL(Service::APT::AppletManager)

namespace Service::APT {

struct AppletTitleData {
    // There are two possible applet ids for each applet.
    std::array<AppletId, 2> applet_ids;

    // There's a specific TitleId per region for each applet.
    static constexpr std::size_t NumRegions = 7;
    std::array<u64, NumRegions> title_ids;
    std::array<u64, NumRegions> n3ds_title_ids = {0, 0, 0, 0, 0, 0, 0};
};

static constexpr std::size_t NumApplets = 29;
static constexpr std::array<AppletTitleData, NumApplets> applet_titleids = {{
    {{AppletId::HomeMenu, AppletId::None},
     {0x4003000008202, 0x4003000008F02, 0x4003000009802, 0x4003000008202, 0x400300000A102,
      0x400300000A902, 0x400300000B102}},
    {{AppletId::AlternateMenu, AppletId::None},
     {0x4003000008102, 0x4003000008102, 0x4003000008102, 0x4003000008102, 0x4003000008102,
      0x4003000008102, 0x4003000008102}},
    {{AppletId::Camera, AppletId::None},
     {0x4003000008402, 0x4003000009002, 0x4003000009902, 0x4003000008402, 0x400300000A202,
      0x400300000AA02, 0x400300000B202}},
    {{AppletId::FriendList, AppletId::None},
     {0x4003000008D02, 0x4003000009602, 0x4003000009F02, 0x4003000008D02, 0x400300000A702,
      0x400300000AF02, 0x400300000B702}},
    {{AppletId::GameNotes, AppletId::None},
     {0x4003000008702, 0x4003000009302, 0x4003000009C02, 0x4003000008702, 0x400300000A502,
      0x400300000AD02, 0x400300000B502}},
    {{AppletId::InternetBrowser, AppletId::None},
     {0x4003000008802, 0x4003000009402, 0x4003000009D02, 0x4003000008802, 0x400300000A602,
      0x400300000AE02, 0x400300000B602},
     {0x4003020008802, 0x4003020009402, 0x4003020009D02, 0x4003020008802, 0, 0x400302000AE02, 0}},
    {{AppletId::InstructionManual, AppletId::None},
     {0x4003000008602, 0x4003000009202, 0x4003000009B02, 0x4003000008602, 0x400300000A402,
      0x400300000AC02, 0x400300000B402}},
    {{AppletId::Notifications, AppletId::None},
     {0x4003000008E02, 0x4003000009702, 0x400300000A002, 0x4003000008E02, 0x400300000A802,
      0x400300000B002, 0x400300000B802}},
    {{AppletId::Miiverse, AppletId::None},
     {0x400300000BC02, 0x400300000BD02, 0x400300000BE02, 0x400300000BC02, 0x4003000009E02,
      0x4003000009502, 0x400300000B902}},
    // These values obtained from an older NS dump firmware 4.5
    {{AppletId::MiiversePost, AppletId::None},
     {0x400300000BA02, 0x400300000BA02, 0x400300000BA02, 0x400300000BA02, 0x400300000BA02,
      0x400300000BA02, 0x400300000BA02}},
    // {AppletId::MiiversePost, AppletId::None, 0x4003000008302, 0x4003000008B02, 0x400300000BA02,
    //  0x4003000008302, 0x0, 0x0, 0x0},
    {{AppletId::AmiiboSettings, AppletId::None},
     {0x4003000009502, 0x4003000009E02, 0x400300000B902, 0x4003000009502, 0x0, 0x4003000008C02,
      0x400300000BF02}},
    {{AppletId::SoftwareKeyboard1, AppletId::SoftwareKeyboard2},
     {0x400300000C002, 0x400300000C802, 0x400300000D002, 0x400300000C002, 0x400300000D802,
      0x400300000DE02, 0x400300000E402}},
    {{AppletId::Ed1, AppletId::Ed2},
     {0x400300000C102, 0x400300000C902, 0x400300000D102, 0x400300000C102, 0x400300000D902,
      0x400300000DF02, 0x400300000E502}},
    {{AppletId::PnoteApp, AppletId::PnoteApp2},
     {0x400300000C302, 0x400300000CB02, 0x400300000D302, 0x400300000C302, 0x400300000DB02,
      0x400300000E102, 0x400300000E702}},
    {{AppletId::SnoteApp, AppletId::SnoteApp2},
     {0x400300000C402, 0x400300000CC02, 0x400300000D402, 0x400300000C402, 0x400300000DC02,
      0x400300000E202, 0x400300000E802}},
    {{AppletId::Error, AppletId::Error2},
     {0x400300000C502, 0x400300000C502, 0x400300000C502, 0x400300000C502, 0x400300000CF02,
      0x400300000CF02, 0x400300000CF02}},
    {{AppletId::Mint, AppletId::Mint2},
     {0x400300000C602, 0x400300000CE02, 0x400300000D602, 0x400300000C602, 0x400300000DD02,
      0x400300000E302, 0x400300000E902}},
    {{AppletId::Extrapad, AppletId::Extrapad2},
     {0x400300000CD02, 0x400300000CD02, 0x400300000CD02, 0x400300000CD02, 0x400300000D502,
      0x400300000D502, 0x400300000D502}},
    {{AppletId::Memolib, AppletId::Memolib2},
     {0x400300000F602, 0x400300000F602, 0x400300000F602, 0x400300000F602, 0x400300000F602,
      0x400300000F602, 0x400300000F602}},
    // TODO(Subv): Fill in the rest of the titleids
}};

static u64 GetTitleIdForApplet(AppletId id, u32 region_value) {
    ASSERT_MSG(id != AppletId::None, "Invalid applet id");

    auto itr = std::find_if(applet_titleids.begin(), applet_titleids.end(),
                            [id](const AppletTitleData& data) {
                                return data.applet_ids[0] == id || data.applet_ids[1] == id;
                            });

    ASSERT_MSG(itr != applet_titleids.end(), "Unknown applet id 0x{:#05X}", id);

    auto n3ds_title_id = itr->n3ds_title_ids[region_value];
    if (n3ds_title_id != 0 && Settings::values.is_new_3ds.GetValue()) {
        return n3ds_title_id;
    }
    return itr->title_ids[region_value];
}

AppletManager::AppletSlot AppletManager::GetAppletSlotFromId(AppletId id) {
    if (id == AppletId::Application) {
        if (GetAppletSlot(AppletSlot::Application)->applet_id != AppletId::None)
            return AppletSlot::Application;

        return AppletSlot::Error;
    }

    if (id == AppletId::AnySystemApplet) {
        if (GetAppletSlot(AppletSlot::SystemApplet)->applet_id != AppletId::None)
            return AppletSlot::SystemApplet;

        // The Home Menu is also a system applet, but it lives in its own slot to be able to run
        // concurrently with other system applets.
        if (GetAppletSlot(AppletSlot::HomeMenu)->applet_id != AppletId::None)
            return AppletSlot::HomeMenu;

        return AppletSlot::Error;
    }

    if (id == AppletId::AnyLibraryApplet || id == AppletId::AnySysLibraryApplet) {
        auto slot_data = GetAppletSlot(AppletSlot::LibraryApplet);
        if (slot_data->applet_id == AppletId::None)
            return AppletSlot::Error;

        auto applet_pos = static_cast<AppletPos>(slot_data->attributes.applet_pos.Value());
        if ((id == AppletId::AnyLibraryApplet && applet_pos == AppletPos::Library) ||
            (id == AppletId::AnySysLibraryApplet && applet_pos == AppletPos::SysLibrary))
            return AppletSlot::LibraryApplet;

        return AppletSlot::Error;
    }

    if (id == AppletId::HomeMenu || id == AppletId::AlternateMenu) {
        if (GetAppletSlot(AppletSlot::HomeMenu)->applet_id != AppletId::None)
            return AppletSlot::HomeMenu;

        return AppletSlot::Error;
    }

    for (std::size_t slot = 0; slot < applet_slots.size(); ++slot) {
        if (applet_slots[slot].applet_id == id) {
            return static_cast<AppletSlot>(slot);
        }
    }

    return AppletSlot::Error;
}

AppletManager::AppletSlot AppletManager::GetAppletSlotFromAttributes(AppletAttributes attributes) {
    // Mapping from AppletPos to AppletSlot
    static constexpr std::array<AppletSlot, 6> applet_position_slots = {
        AppletSlot::Application,   AppletSlot::LibraryApplet, AppletSlot::SystemApplet,
        AppletSlot::LibraryApplet, AppletSlot::Error,         AppletSlot::LibraryApplet};

    auto applet_pos = attributes.applet_pos;
    if (applet_pos >= applet_position_slots.size())
        return AppletSlot::Error;

    auto slot = applet_position_slots[applet_pos];
    if (slot == AppletSlot::Error)
        return AppletSlot::Error;

    // The Home Menu is a system applet, however, it has its own applet slot so that it can run
    // concurrently with other system applets.
    if (slot == AppletSlot::SystemApplet && attributes.is_home_menu)
        return AppletSlot::HomeMenu;

    return slot;
}

AppletManager::AppletSlot AppletManager::GetAppletSlotFromPos(AppletPos pos) {
    AppletId applet_id;
    switch (pos) {
    case AppletPos::Application:
        applet_id = AppletId::Application;
        break;
    case AppletPos::Library:
        applet_id = AppletId::AnyLibraryApplet;
        break;
    case AppletPos::System:
        applet_id = AppletId::AnySystemApplet;
        break;
    case AppletPos::SysLibrary:
        applet_id = AppletId::AnySysLibraryApplet;
        break;
    default:
        return AppletSlot::Error;
    }
    return GetAppletSlotFromId(applet_id);
}

void AppletManager::CancelAndSendParameter(const MessageParameter& parameter) {
    LOG_DEBUG(
        Service_APT, "Sending parameter from {:03X} to {:03X} with signal {:08X} and size {:08X}",
        parameter.sender_id, parameter.destination_id, parameter.signal, parameter.buffer.size());

    // If the applet is being HLEd, send directly to the applet.
    if (auto dest_applet = HLE::Applets::Applet::Get(parameter.destination_id)) {
        dest_applet->ReceiveParameter(parameter);
    } else {
        // Otherwise, send the parameter the LLE way.
        next_parameter = parameter;

        // Signal the event to let the receiver know that a new parameter is ready to be read
        auto slot = GetAppletSlotFromId(parameter.destination_id);
        if (slot != AppletSlot::Error) {
            GetAppletSlot(slot)->parameter_event->Signal();
        } else {
            LOG_DEBUG(Service_APT, "No applet was registered with ID {:03X}",
                      parameter.destination_id);
        }
    }
}

ResultCode AppletManager::SendParameter(const MessageParameter& parameter) {
    // A new parameter can not be sent if the previous one hasn't been consumed yet
    if (next_parameter) {
        LOG_WARNING(Service_APT, "Parameter from {:03X} to {:03X} blocked by pending parameter.",
                    parameter.sender_id, parameter.destination_id);
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    CancelAndSendParameter(parameter);
    return RESULT_SUCCESS;
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

    auto parameter = *next_parameter;

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
    auto cancellation_success =
        next_parameter && (!check_sender || next_parameter->sender_id == sender_appid) &&
        (!check_receiver || next_parameter->destination_id == receiver_appid);

    if (cancellation_success)
        next_parameter = {};

    return cancellation_success;
}

ResultVal<AppletManager::GetLockHandleResult> AppletManager::GetLockHandle(
    AppletAttributes attributes) {
    auto corrected_attributes = attributes;
    if (attributes.applet_pos == static_cast<u32>(AppletPos::Library) ||
        attributes.applet_pos == static_cast<u32>(AppletPos::SysLibrary) ||
        attributes.applet_pos == static_cast<u32>(AppletPos::AutoLibrary)) {
        auto corrected_pos = last_library_launcher_slot == AppletSlot::Application
                                 ? AppletPos::Library
                                 : AppletPos::SysLibrary;
        corrected_attributes.applet_pos.Assign(static_cast<u32>(corrected_pos));
        LOG_DEBUG(Service_APT, "Corrected applet attributes from {:08X} to {:08X}", attributes.raw,
                  corrected_attributes.raw);
    }

    return MakeResult<AppletManager::GetLockHandleResult>({corrected_attributes, 0, lock});
}

ResultVal<AppletManager::InitializeResult> AppletManager::Initialize(AppletId app_id,
                                                                     AppletAttributes attributes) {
    auto slot = GetAppletSlotFromAttributes(attributes);
    // Note: The real NS service does not check if the attributes value is valid before accessing
    // the data in the array
    ASSERT_MSG(slot != AppletSlot::Error, "Invalid application attributes");

    auto slot_data = GetAppletSlot(slot);
    if (slot_data->registered) {
        LOG_WARNING(Service_APT, "Applet attempted to register in occupied slot {:02X}", slot);
        return ResultCode(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    LOG_DEBUG(Service_APT, "Initializing applet with ID {:03X} and attributes {:08X}.", app_id,
              attributes.raw);
    slot_data->applet_id = static_cast<AppletId>(app_id);
    // Note: In the real console the title id of a given applet slot is set by the APT module when
    // calling StartApplication.
    slot_data->title_id = system.Kernel().GetCurrentProcess()->codeset->program_id;
    slot_data->attributes.raw = attributes.raw;

    // Applications need to receive a Wakeup signal to actually start up, this signal is usually
    // sent by the Home Menu after starting the app by way of APT::WakeupApplication. However,
    // if nothing is running yet the signal should be sent by APT::Initialize itself.
    if (active_slot == AppletSlot::Error) {
        active_slot = slot;

        // Wake up the application.
        SendParameter({
            .sender_id = AppletId::None,
            .destination_id = app_id,
            .signal = SignalType::Wakeup,
        });
    }

    return MakeResult<InitializeResult>(
        {slot_data->notification_event, slot_data->parameter_event});
}

ResultCode AppletManager::Enable(AppletAttributes attributes) {
    auto slot = GetAppletSlotFromAttributes(attributes);
    if (slot == AppletSlot::Error) {
        LOG_WARNING(Service_APT,
                    "Attempted to register with attributes {:08X}, but could not find slot.",
                    attributes.raw);
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    LOG_DEBUG(Service_APT, "Registering applet with attributes {:08X}.", attributes.raw);
    auto slot_data = GetAppletSlot(slot);
    slot_data->registered = true;

    // Send any outstanding parameters to the newly-registered application
    if (delayed_parameter && delayed_parameter->destination_id == slot_data->applet_id) {
        // TODO: Real APT would loop trying to send the parameter until it succeeds,
        // essentially waiting for existing parameters to be delivered.
        CancelAndSendParameter(*delayed_parameter);
        delayed_parameter.reset();
    }

    return RESULT_SUCCESS;
}

bool AppletManager::IsRegistered(AppletId app_id) {
    auto slot = GetAppletSlotFromId(app_id);
    return slot != AppletSlot::Error && GetAppletSlot(slot)->registered;
}

ResultCode AppletManager::PrepareToStartLibraryApplet(AppletId applet_id) {
    // The real APT service returns an error if there's a pending APT parameter when this function
    // is called.
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    if (GetAppletSlot(AppletSlot::LibraryApplet)->registered) {
        return {ErrorDescription::AlreadyExists, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    last_library_launcher_slot = active_slot;
    last_prepared_library_applet = applet_id;

    auto cfg = Service::CFG::GetModule(system);
    auto process =
        NS::LaunchTitle(FS::MediaType::NAND, GetTitleIdForApplet(applet_id, cfg->GetRegionValue()));
    if (process) {
        return RESULT_SUCCESS;
    }

    // If we weren't able to load the native applet title, try to fallback to an HLE implementation.
    auto applet = HLE::Applets::Applet::Get(applet_id);
    if (applet) {
        LOG_WARNING(Service_APT, "applet has already been started id={:03X}", applet_id);
        return RESULT_SUCCESS;
    } else {
        auto parent = GetAppletSlotId(last_library_launcher_slot);
        LOG_DEBUG(Service_APT, "Creating HLE applet {:03X} with parent {:03X}", applet_id, parent);
        return HLE::Applets::Applet::Create(applet_id, parent, false, shared_from_this());
    }
}

ResultCode AppletManager::PreloadLibraryApplet(AppletId applet_id) {
    if (GetAppletSlot(AppletSlot::LibraryApplet)->registered) {
        return {ErrorDescription::AlreadyExists, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    last_library_launcher_slot = active_slot;
    last_prepared_library_applet = applet_id;

    auto cfg = Service::CFG::GetModule(system);
    auto process =
        NS::LaunchTitle(FS::MediaType::NAND, GetTitleIdForApplet(applet_id, cfg->GetRegionValue()));
    if (process) {
        return RESULT_SUCCESS;
    }

    // If we weren't able to load the native applet title, try to fallback to an HLE implementation.
    auto applet = HLE::Applets::Applet::Get(applet_id);
    if (applet) {
        LOG_WARNING(Service_APT, "applet has already been started id={:08X}", applet_id);
        return RESULT_SUCCESS;
    } else {
        auto parent = GetAppletSlotId(last_library_launcher_slot);
        LOG_DEBUG(Service_APT, "Creating HLE applet {:03X} with parent {:03X}", applet_id, parent);
        return HLE::Applets::Applet::Create(applet_id, parent, true, shared_from_this());
    }
}

ResultCode AppletManager::FinishPreloadingLibraryApplet(AppletId applet_id) {
    // TODO(Subv): This function should fail depending on the applet preparation state.
    GetAppletSlot(AppletSlot::LibraryApplet)->loaded = true;
    return RESULT_SUCCESS;
}

ResultCode AppletManager::StartLibraryApplet(AppletId applet_id,
                                             std::shared_ptr<Kernel::Object> object,
                                             const std::vector<u8>& buffer) {
    active_slot = AppletSlot::LibraryApplet;

    auto send_res = SendParameter({
        .sender_id = GetAppletSlotId(last_library_launcher_slot),
        .destination_id = applet_id,
        .signal = SignalType::Wakeup,
        .object = std::move(object),
        .buffer = buffer,
    });
    if (send_res.IsError()) {
        active_slot = last_library_launcher_slot;
        return send_res;
    }

    return RESULT_SUCCESS;
}

ResultCode AppletManager::PrepareToCloseLibraryApplet(bool not_pause, bool exiting,
                                                      bool jump_home) {
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
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
                                             const std::vector<u8>& buffer) {
    auto slot = GetAppletSlot(AppletSlot::LibraryApplet);
    auto destination_id = GetAppletSlotId(last_library_launcher_slot);

    active_slot = last_library_launcher_slot;

    MessageParameter param = {
        .sender_id = slot->applet_id,
        .destination_id = destination_id,
        .signal = library_applet_closing_command,
        .object = std::move(object),
        .buffer = buffer,
    };

    if (library_applet_closing_command != SignalType::WakeupByPause) {
        CancelAndSendParameter(param);
        // TODO: Terminate the running applet title
        slot->Reset();
    } else {
        SendParameter(param);
    }

    return RESULT_SUCCESS;
}

ResultCode AppletManager::CancelLibraryApplet(bool app_exiting) {
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    auto slot = GetAppletSlot(AppletSlot::LibraryApplet);
    if (!slot->registered) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    return SendParameter({
        .sender_id = GetAppletSlotId(last_library_launcher_slot),
        .destination_id = slot->applet_id,
        .signal = SignalType::WakeupByCancel,
    });
}

ResultCode AppletManager::PrepareToStartSystemApplet(AppletId applet_id) {
    // The real APT service returns an error if there's a pending APT parameter when this function
    // is called.
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    last_system_launcher_slot = active_slot;
    return RESULT_SUCCESS;
}

ResultCode AppletManager::StartSystemApplet(AppletId applet_id,
                                            std::shared_ptr<Kernel::Object> object,
                                            const std::vector<u8>& buffer) {
    auto source_applet_id = AppletId::None;
    if (last_system_launcher_slot != AppletSlot::Error) {
        const auto slot_data = GetAppletSlot(last_system_launcher_slot);
        source_applet_id = slot_data->applet_id;

        // If a system applet is launching another system applet, reset the slot to avoid conflicts.
        // This is needed because system applets won't necessarily call CloseSystemApplet before
        // exiting.
        if (last_system_launcher_slot == AppletSlot::SystemApplet) {
            slot_data->Reset();
        }
    }

    // If a system applet is not already registered, it is started by APT.
    const auto slot_id =
        applet_id == AppletId::HomeMenu ? AppletSlot::HomeMenu : AppletSlot::SystemApplet;
    if (!GetAppletSlot(slot_id)->registered) {
        auto cfg = Service::CFG::GetModule(system);
        auto process = NS::LaunchTitle(FS::MediaType::NAND,
                                       GetTitleIdForApplet(applet_id, cfg->GetRegionValue()));
        if (!process) {
            // TODO: Find the right error code.
            return {ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotSupported,
                    ErrorLevel::Permanent};
        }
    }

    active_slot = slot_id;

    SendApplicationParameterAfterRegistration({
        .sender_id = source_applet_id,
        .destination_id = applet_id,
        .signal = SignalType::Wakeup,
        .object = std::move(object),
        .buffer = buffer,
    });

    return RESULT_SUCCESS;
}

ResultCode AppletManager::PrepareToCloseSystemApplet() {
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    return RESULT_SUCCESS;
}

ResultCode AppletManager::CloseSystemApplet(std::shared_ptr<Kernel::Object> object,
                                            const std::vector<u8>& buffer) {
    ASSERT_MSG(active_slot == AppletSlot::HomeMenu || active_slot == AppletSlot::SystemApplet,
               "Attempting to close a system applet from a non-system applet.");

    auto slot = GetAppletSlot(active_slot);

    active_slot = last_system_launcher_slot;

    // TODO: Send a parameter to the application only if the application ordered the applet to
    // close.

    // TODO: Terminate the running applet title
    slot->Reset();

    return RESULT_SUCCESS;
}

ResultVal<AppletManager::AppletInfo> AppletManager::GetAppletInfo(AppletId app_id) {
    auto slot = GetAppletSlotFromId(app_id);
    if (slot == AppletSlot::Error) {
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                          ErrorLevel::Status);
    }

    auto slot_data = GetAppletSlot(slot);
    if (!slot_data->registered) {
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                          ErrorLevel::Status);
    }

    // TODO: Basic heuristic to guess media type, needs proper implementation.
    auto media_type = ((slot_data->title_id >> 32) & 0xFFFFFFFF) == 0x00040000
                          ? Service::FS::MediaType::SDMC
                          : Service::FS::MediaType::NAND;
    return MakeResult<AppletInfo>({slot_data->title_id, media_type, slot_data->registered,
                                   slot_data->loaded, slot_data->attributes.raw});
}

ResultCode AppletManager::PrepareToDoApplicationJump(u64 title_id, FS::MediaType media_type,
                                                     ApplicationJumpFlags flags) {
    // A running application can not launch another application directly because the applet state
    // for the Application slot is already in use. The way this is implemented in hardware is to
    // launch the Home Menu and tell it to launch our desired application.

    ASSERT_MSG(flags != ApplicationJumpFlags::UseStoredParameters,
               "Unimplemented application jump flags 1");

    // Save the title data to send it to the Home Menu when DoApplicationJump is called.
    auto application_slot_data = GetAppletSlot(AppletSlot::Application);
    app_jump_parameters.current_title_id = application_slot_data->title_id;
    // TODO(Subv): Retrieve the correct media type of the currently-running application. For now
    // just assume NAND.
    app_jump_parameters.current_media_type = FS::MediaType::NAND;
    app_jump_parameters.next_title_id = flags == ApplicationJumpFlags::UseCurrentParameters
                                            ? application_slot_data->title_id
                                            : title_id;
    app_jump_parameters.next_media_type = media_type;
    app_jump_parameters.flags = flags;

    // Note: The real console uses the Home Menu to perform the application jump, therefore the menu
    // needs to be running. The real APT module starts the Home Menu here if it's not already
    // running, we don't have to do this. See `EnsureHomeMenuLoaded` for launching the Home Menu.
    return RESULT_SUCCESS;
}

ResultCode AppletManager::DoApplicationJump(const DeliverArg& arg) {
    // Note: The real console uses the Home Menu to perform the application jump, it goes
    // OldApplication->Home Menu->NewApplication. We do not need to use the Home Menu to do this so
    // we launch the new application directly. In the real APT service, the Home Menu must be
    // running to do this, otherwise error 0xC8A0CFF0 is returned.

    auto application_slot_data = GetAppletSlot(AppletSlot::Application);
    auto title_id = application_slot_data->title_id;
    application_slot_data->Reset();

    // Set the delivery parameters.
    deliver_arg = arg;
    if (app_jump_parameters.flags != ApplicationJumpFlags::UseCurrentParameters) {
        // The source program ID is not updated when using flags 0x2.
        deliver_arg->source_program_id = title_id;
    }

    // TODO(Subv): Terminate the current Application.

    // Note: The real console sends signal 17 (WakeupToLaunchApplication) to the Home Menu, this
    // prompts it to call GetProgramIdOnApplicationJump and
    // PrepareToStartApplication/StartApplication on the title to launch.
    active_slot = AppletSlot::Application;

    // Perform a soft-reset if we're trying to relaunch the same title.
    // TODO(Subv): Note that this reboots the entire emulated system, a better way would be to
    // simply re-launch the title without closing all services, but this would only work for
    // installed titles since we have no way of getting the file path of an arbitrary game dump
    // based only on the title id.

    auto new_path = Service::AM::GetTitleContentPath(app_jump_parameters.next_media_type,
                                                     app_jump_parameters.next_title_id);
    if (new_path.empty() || !FileUtil::Exists(new_path)) {
        LOG_CRITICAL(
            Service_APT,
            "Failed to find title during application jump: {} Resetting current title instead.",
            new_path);
        new_path.clear();
    }

    system.RequestReset(new_path);
    return RESULT_SUCCESS;

    // Launch the title directly.
    // The emulator does not suport terminating old processes, would require a lot of cleanup
    // This code is left commented for when this is implemented, for now we cannot use NS
    // as the old process resources would interfere with the new ones
    /*
    auto process =
        NS::LaunchTitle(app_jump_parameters.next_media_type, app_jump_parameters.next_title_id);
    if (!process) {
        LOG_CRITICAL(Service_APT, "Failed to launch title during application jump, exiting.");
        system.RequestShutdown();
    }
    return RESULT_SUCCESS;
    */
}

ResultCode AppletManager::PrepareToStartApplication(u64 title_id, FS::MediaType media_type) {
    if (active_slot == AppletSlot::Error ||
        GetAppletSlot(active_slot)->attributes.applet_pos != static_cast<u32>(AppletPos::System)) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    // TODO(Subv): This should return 0xc8a0cff0 if the applet preparation state is already set

    if (GetAppletSlot(AppletSlot::Application)->registered) {
        return {ErrorDescription::AlreadyExists, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    ASSERT_MSG(!app_start_parameters,
               "Trying to prepare an application when another is already prepared");

    app_start_parameters.emplace();
    app_start_parameters->next_title_id = title_id;
    app_start_parameters->next_media_type = media_type;

    return RESULT_SUCCESS;
}

ResultCode AppletManager::StartApplication(const std::vector<u8>& parameter,
                                           const std::vector<u8>& hmac, bool paused) {
    // The delivery argument is always unconditionally set.
    deliver_arg.emplace(DeliverArg{parameter, hmac});

    // Note: APT first checks if we can launch the application via AM::CheckDemoLaunchRights and
    // returns 0xc8a12403 if we can't. We intentionally do not implement that check.

    // TODO(Subv): The APT service performs several checks here related to the exheader flags of the
    // process we're launching and other things like title id blacklists. We do not yet implement
    // any of that.

    // TODO(Subv): The real APT service doesn't seem to check whether the titleid to launch is set
    // or not, it either launches NATIVE_FIRM if some internal state is set, or fails when calling
    // PM::LaunchTitle. We should research more about that.
    ASSERT_MSG(app_start_parameters, "Trying to start an application without preparing it first.");

    active_slot = AppletSlot::Application;

    // Launch the title directly.
    auto process =
        NS::LaunchTitle(app_start_parameters->next_media_type, app_start_parameters->next_title_id);
    if (!process) {
        LOG_CRITICAL(Service_APT, "Failed to launch title during application start, exiting.");
        system.RequestShutdown();
    }

    app_start_parameters.reset();

    if (!paused) {
        return WakeupApplication();
    }

    return RESULT_SUCCESS;
}

ResultCode AppletManager::WakeupApplication() {
    // Send a Wakeup signal via the apt parameter to the application once it registers itself.
    // The real APT service does this by spin waiting on another thread until the application is
    // registered.
    SendApplicationParameterAfterRegistration({.sender_id = AppletId::HomeMenu,
                                               .destination_id = AppletId::Application,
                                               .signal = SignalType::Wakeup});

    return RESULT_SUCCESS;
}

void AppletManager::SendApplicationParameterAfterRegistration(const MessageParameter& parameter) {
    auto slot = GetAppletSlotFromId(parameter.destination_id);

    // If the application is already registered, immediately send the parameter
    if (slot != AppletSlot::Error && GetAppletSlot(slot)->registered) {
        CancelAndSendParameter(parameter);
        return;
    }

    // Otherwise queue it until the Application calls APT::Enable
    delayed_parameter = parameter;
}

void AppletManager::EnsureHomeMenuLoaded() {
    // TODO(Subv): The real APT service sends signal 12 (WakeupByCancel) to the currently running
    // System applet, waits for it to finish, and then launches the Home Menu.
    ASSERT_MSG(!GetAppletSlot(AppletSlot::SystemApplet)->registered,
               "A system applet is already running");

    if (GetAppletSlot(AppletSlot::HomeMenu)->registered) {
        // The Home Menu is already running.
        return;
    }

    auto cfg = Service::CFG::GetModule(system);
    ASSERT_MSG(cfg, "CFG Module missing!");

    auto menu_title_id = GetTitleIdForApplet(AppletId::HomeMenu, cfg->GetRegionValue());
    auto process = NS::LaunchTitle(FS::MediaType::NAND, menu_title_id);
    if (!process) {
        LOG_WARNING(Service_APT,
                    "The Home Menu failed to launch, application jumping will not work.");
    }
}

AppletManager::AppletManager(Core::System& system) : system(system) {
    lock = system.Kernel().CreateMutex(false, "APT_U:Lock");
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
