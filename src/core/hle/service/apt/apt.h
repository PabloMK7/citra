// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/swap.h"

#include "core/hle/kernel/kernel.h"

namespace Service {

class Interface;

namespace APT {

/// Holds information about the parameters used in Send/Glance/ReceiveParameter
struct MessageParameter {
    u32 sender_id = 0;
    u32 destination_id = 0;
    u32 signal = 0;
    Kernel::SharedPtr<Kernel::Object> object = nullptr;
    std::vector<u8> buffer;
};

/// Holds information about the parameters used in StartLibraryApplet
struct AppletStartupParameter {
    Kernel::SharedPtr<Kernel::Object> object = nullptr;
    std::vector<u8> buffer;
};

/// Used by the application to pass information about the current framebuffer to applets.
struct CaptureBufferInfo {
    u32_le size;
    u8 is_3d;
    INSERT_PADDING_BYTES(0x3); // Padding for alignment
    u32_le top_screen_left_offset;
    u32_le top_screen_right_offset;
    u32_le top_screen_format;
    u32_le bottom_screen_left_offset;
    u32_le bottom_screen_right_offset;
    u32_le bottom_screen_format;
};
static_assert(sizeof(CaptureBufferInfo) == 0x20, "CaptureBufferInfo struct has incorrect size");

/// Signals used by APT functions
enum class SignalType : u32 {
    None              = 0x0,
    AppJustStarted    = 0x1,
    LibAppJustStarted = 0x2,
    LibAppFinished    = 0x3,
    LibAppClosed      = 0xA,
    ReturningToApp    = 0xB,
    ExitingApp        = 0xC,
};

/// App Id's used by APT functions
enum class AppletId : u32 {
    HomeMenu           = 0x101,
    AlternateMenu      = 0x103,
    Camera             = 0x110,
    FriendsList        = 0x112,
    GameNotes          = 0x113,
    InternetBrowser    = 0x114,
    InstructionManual  = 0x115,
    Notifications      = 0x116,
    Miiverse           = 0x117,
    SoftwareKeyboard1  = 0x201,
    Ed1                = 0x202,
    PnoteApp           = 0x204,
    SnoteApp           = 0x205,
    Error              = 0x206,
    Mint               = 0x207,
    Extrapad           = 0x208,
    Memolib            = 0x209,
    Application        = 0x300,
    AnyLibraryApplet   = 0x400,
    SoftwareKeyboard2  = 0x401,
    Ed2                = 0x402,
};

enum class StartupArgumentType : u32 {
    OtherApp   = 0,
    Restart    = 1,
    OtherMedia = 2,
};

/// Send a parameter to the currently-running application, which will read it via ReceiveParameter
void SendParameter(const MessageParameter& parameter);

/**
 * APT::Initialize service function
 * Service function that initializes the APT process for the running application
 *  Outputs:
 *      1 : Result of the function, 0 on success, otherwise error code
 *      3 : Handle to the notification event
 *      4 : Handle to the pause event
 */
void Initialize(Service::Interface* self);

/**
 * APT::GetSharedFont service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Virtual address of where shared font will be loaded in memory
 *      4 : Handle to shared font memory
 */
void GetSharedFont(Service::Interface* self);

/**
 * APT::NotifyToWait service function
 *  Inputs:
 *      1 : AppID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void NotifyToWait(Service::Interface* self);

/**
 * APT::GetLockHandle service function
 *  Inputs:
 *      1 : Applet attributes
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Applet attributes
 *      3 : Power button state
 *      4 : IPC handle descriptor
 *      5 : APT mutex handle
 */
void GetLockHandle(Service::Interface* self);

/**
 * APT::Enable service function
 *  Inputs:
 *      1 : Applet attributes
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void Enable(Service::Interface* self);

/**
 * APT::GetAppletManInfo service function.
 *  Inputs:
 *      1 : Unknown
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Unknown u32 value
 *      3 : Unknown u8 value
 *      4 : Home Menu AppId
 *      5 : AppID of currently active app
 */
void GetAppletManInfo(Service::Interface* self);

/**
 * APT::GetAppletInfo service function.
 *  Inputs:
 *      1 : AppId
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2-3 : Title ID
 *      4 : Media Type
 *      5 : Registered
 *      6 : Loaded
 *      7 : Attributes
 */
void GetAppletInfo(Service::Interface* self);

/**
 * APT::IsRegistered service function. This returns whether the specified AppID is registered with NS yet.
 * An AppID is "registered" once the process associated with the AppID uses APT:Enable. Home Menu uses this
 * command to determine when the launched process is running and to determine when to stop using GSP etc,
 * while displaying the "Nintendo 3DS" loading screen.
 *  Inputs:
 *      1 : AppID
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output, 0 = not registered, 1 = registered.
 */
void IsRegistered(Service::Interface* self);

void InquireNotification(Service::Interface* self);

/**
 * APT::SendParameter service function. This sets the parameter data state.
 * Inputs:
 *     1 : Source AppID
 *     2 : Destination AppID
 *     3 : Signal type
 *     4 : Parameter buffer size, max size is 0x1000 (this can be zero)
 *     5 : Value
 *     6 : Handle to the destination process, likely used for shared memory (this can be zero)
 *     7 : (Size<<14) | 2
 *     8 : Input parameter buffer ptr
 * Outputs:
 *     0 : Return Header
 *     1 : Result of function, 0 on success, otherwise error code
*/
void SendParameter(Service::Interface* self);

/**
 * APT::ReceiveParameter service function. This returns the current parameter data from NS state,
 * from the source process which set the parameters. Once finished, NS will clear a flag in the NS
 * state so that this command will return an error if this command is used again if parameters were
 * not set again. This is called when the second Initialize event is triggered. It returns a signal
 * type indicating why it was triggered.
 *  Inputs:
 *      1 : AppID
 *      2 : Parameter buffer size, max size is 0x1000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : AppID of the process which sent these parameters
 *      3 : Signal type
 *      4 : Actual parameter buffer size, this is <= to the the input size
 *      5 : Value
 *      6 : Handle from the source process which set the parameters, likely used for shared memory
 *      7 : Size
 *      8 : Output parameter buffer ptr
 */
void ReceiveParameter(Service::Interface* self);

/**
 * APT::GlanceParameter service function. This is exactly the same as APT_U::ReceiveParameter
 * (except for the word value prior to the output handle), except this will not clear the flag
 * (except when responseword[3]==8 || responseword[3]==9) in NS state.
 *  Inputs:
 *      1 : AppID
 *      2 : Parameter buffer size, max size is 0x1000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Unknown, for now assume AppID of the process which sent these parameters
 *      3 : Unknown, for now assume Signal type
 *      4 : Actual parameter buffer size, this is <= to the the input size
 *      5 : Value
 *      6 : Handle from the source process which set the parameters, likely used for shared memory
 *      7 : Size
 *      8 : Output parameter buffer ptr
 */
void GlanceParameter(Service::Interface* self);

/**
 * APT::CancelParameter service function. When the parameter data is available, and when the above
 * specified fields match the ones in NS state(for the ones where the checks are enabled), this
 * clears the flag which indicates that parameter data is available
 * (same flag cleared by APT:ReceiveParameter).
 *  Inputs:
 *      1 : Flag, when non-zero NS will compare the word after this one with a field in the NS state.
 *      2 : Unknown, this is the same as the first unknown field returned by APT:ReceiveParameter.
 *      3 : Flag, when non-zero NS will compare the word after this one with a field in the NS state.
 *      4 : AppID
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Status flag, 0 = failure due to no parameter data being available, or the above enabled
 *          fields don't match the fields in NS state. 1 = success.
 */
void CancelParameter(Service::Interface* self);

/**
 * APT::PrepareToStartApplication service function. When the input title-info programID is zero,
 * NS will load the actual program ID via AMNet:GetTitleIDList. After doing some checks with the
 * programID, NS will then set a NS state flag to value 1, then set the programID for AppID
 * 0x300(application) to the input program ID(or the one from GetTitleIDList). A media-type field
 * in the NS state is also set to the input media-type value
 * (other state fields are set at this point as well). With 8.0.0-18, NS will set an u8 NS state
 * field to value 1 when input flags bit8 is set
 *  Inputs:
 *    1-4 : 0x10-byte title-info struct
 *      4 : Flags
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PrepareToStartApplication(Service::Interface* self);

/**
 * APT::StartApplication service function. Buf0 is copied to NS FIRMparams+0x0, then Buf1 is copied
 * to the NS FIRMparams+0x480. Then the application is launched.
 * Inputs:
 *     1 : Buffer 0 size, max size is 0x300
 *     2 : Buffer 1 size, max size is 0x20 (this can be zero)
 *     3 : u8 flag
 *     4 : (Size0<<14) | 2
 *     5 : Buffer 0 pointer
 *     6 : (Size1<<14) | 0x802
 *     7 : Buffer 1 pointer
 * Outputs:
 *     0 : Return Header
 *     1 : Result of function, 0 on success, otherwise error code
*/
void StartApplication(Service::Interface* self);

/**
 * APT::AppletUtility service function
 *  Inputs:
 *      1 : Unknown, but clearly used for something
 *      2 : Buffer 1 size (purpose is unknown)
 *      3 : Buffer 2 size (purpose is unknown)
 *      5 : Buffer 1 address (purpose is unknown)
 *      65 : Buffer 2 address (purpose is unknown)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void AppletUtility(Service::Interface* self);

/**
 * APT::SetAppCpuTimeLimit service function
 *  Inputs:
 *      1 : Value, must be one
 *      2 : Percentage of CPU time from 5 to 80
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetAppCpuTimeLimit(Service::Interface* self);

/**
 * APT::GetAppCpuTimeLimit service function
 *  Inputs:
 *      1 : Value, must be one
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : System core CPU time percentage
 */
void GetAppCpuTimeLimit(Service::Interface* self);

/**
 * APT::PrepareToStartLibraryApplet service function
 *  Inputs:
 *      0 : Command header [0x00180040]
 *      1 : Id of the applet to start
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PrepareToStartLibraryApplet(Service::Interface* self);

/**
 * APT::PreloadLibraryApplet service function
 *  Inputs:
 *      0 : Command header [0x00160040]
 *      1 : Id of the applet to start
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
void PreloadLibraryApplet(Service::Interface* self);

/**
 * APT::StartLibraryApplet service function
 *  Inputs:
 *      0 : Command header [0x001E0084]
 *      1 : Id of the applet to start
 *      2 : Buffer size
 *      3 : Always 0?
 *      4 : Handle passed to the applet
 *      5 : (Size << 14) | 2
 *      6 : Input buffer virtual address
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
void StartLibraryApplet(Service::Interface* self);

/**
 * APT::GetStartupArgument service function
 *  Inputs:
 *      1 : Parameter Size (capped to 0x300)
 *      2 : StartupArgumentType
 *  Outputs:
 *      0 : Return header
 *      1 : u8, Exists (0 = does not exist, 1 = exists)
 */
void GetStartupArgument(Service::Interface* self);

/**
 * APT::SetNSStateField service function
 *  Inputs:
 *      1 : u8 NS state field
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *  Note:
 *      This writes the input u8 to a NS state field.
 */
void SetNSStateField(Service::Interface* self);

/**
 * APT::GetNSStateField service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      8 : u8 NS state field
 *  Note:
 *      This returns a u8 NS state field(which can be set by cmd 0x00550040), at cmdreply+8.
 */
void GetNSStateField(Service::Interface* self);

/**
 * APT::CheckNew3DSApp service function
 *  Outputs:
 *      1: Result code, 0 on success, otherwise error code
 *      2: u8 output: 0 = Old3DS, 1 = New3DS.
 *  Note:
 *  This uses PTMSYSM:CheckNew3DS.
 *  When a certain NS state field is non-zero, the output value is zero,
 *  Otherwise the output is from PTMSYSM:CheckNew3DS.
 *  Normally this NS state field is zero, however this state field is set to 1
 *  when APT:PrepareToStartApplication is used with flags bit8 is set.
 */
void CheckNew3DSApp(Service::Interface* self);

/**
 * Wrapper for PTMSYSM:CheckNew3DS
 * APT::CheckNew3DS service function
 *  Outputs:
 *      1: Result code, 0 on success, otherwise error code
 *      2: u8 output: 0 = Old3DS, 1 = New3DS.
 */
void CheckNew3DS(Service::Interface* self);

/// Initialize the APT service
void Init();

/// Shutdown the APT service
void Shutdown();

} // namespace APT
} // namespace Service
