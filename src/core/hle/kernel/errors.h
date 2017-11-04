// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace Kernel {

namespace ErrCodes {
enum {
    OutOfHandles = 19,
    SessionClosedByRemote = 26,
    PortNameTooLong = 30,
    WrongLockingThread = 31,
    NoPendingSessions = 35,
    WrongPermission = 46,
    InvalidBufferDescriptor = 48,
    MaxConnectionsReached = 52,
};
}

// WARNING: The kernel is quite inconsistent in it's usage of errors code. Make sure to always
// double check that the code matches before re-using the constant.

constexpr ResultCode ERR_OUT_OF_HANDLES(ErrCodes::OutOfHandles, ErrorModule::Kernel,
                                        ErrorSummary::OutOfResource,
                                        ErrorLevel::Permanent); // 0xD8600413
constexpr ResultCode ERR_SESSION_CLOSED_BY_REMOTE(ErrCodes::SessionClosedByRemote, ErrorModule::OS,
                                                  ErrorSummary::Canceled,
                                                  ErrorLevel::Status); // 0xC920181A
constexpr ResultCode ERR_PORT_NAME_TOO_LONG(ErrCodes::PortNameTooLong, ErrorModule::OS,
                                            ErrorSummary::InvalidArgument,
                                            ErrorLevel::Usage); // 0xE0E0181E
constexpr ResultCode ERR_WRONG_PERMISSION(ErrCodes::WrongPermission, ErrorModule::OS,
                                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
constexpr ResultCode ERR_INVALID_BUFFER_DESCRIPTOR(ErrCodes::InvalidBufferDescriptor,
                                                   ErrorModule::OS, ErrorSummary::WrongArgument,
                                                   ErrorLevel::Permanent);
constexpr ResultCode ERR_MAX_CONNECTIONS_REACHED(ErrCodes::MaxConnectionsReached, ErrorModule::OS,
                                                 ErrorSummary::WouldBlock,
                                                 ErrorLevel::Temporary); // 0xD0401834

constexpr ResultCode ERR_NOT_AUTHORIZED(ErrorDescription::NotAuthorized, ErrorModule::OS,
                                        ErrorSummary::WrongArgument,
                                        ErrorLevel::Permanent); // 0xD9001BEA
constexpr ResultCode ERR_INVALID_ENUM_VALUE(ErrorDescription::InvalidEnumValue, ErrorModule::Kernel,
                                            ErrorSummary::InvalidArgument,
                                            ErrorLevel::Permanent); // 0xD8E007ED
constexpr ResultCode ERR_INVALID_ENUM_VALUE_FND(ErrorDescription::InvalidEnumValue,
                                                ErrorModule::FND, ErrorSummary::InvalidArgument,
                                                ErrorLevel::Permanent); // 0xD8E093ED
constexpr ResultCode ERR_INVALID_COMBINATION(ErrorDescription::InvalidCombination, ErrorModule::OS,
                                             ErrorSummary::InvalidArgument,
                                             ErrorLevel::Usage); // 0xE0E01BEE
constexpr ResultCode ERR_INVALID_COMBINATION_KERNEL(ErrorDescription::InvalidCombination,
                                                    ErrorModule::Kernel,
                                                    ErrorSummary::WrongArgument,
                                                    ErrorLevel::Permanent); // 0xD90007EE
constexpr ResultCode ERR_MISALIGNED_ADDRESS(ErrorDescription::MisalignedAddress, ErrorModule::OS,
                                            ErrorSummary::InvalidArgument,
                                            ErrorLevel::Usage); // 0xE0E01BF1
constexpr ResultCode ERR_MISALIGNED_SIZE(ErrorDescription::MisalignedSize, ErrorModule::OS,
                                         ErrorSummary::InvalidArgument,
                                         ErrorLevel::Usage); // 0xE0E01BF2
constexpr ResultCode ERR_OUT_OF_MEMORY(ErrorDescription::OutOfMemory, ErrorModule::Kernel,
                                       ErrorSummary::OutOfResource,
                                       ErrorLevel::Permanent); // 0xD86007F3
constexpr ResultCode ERR_NOT_IMPLEMENTED(ErrorDescription::NotImplemented, ErrorModule::OS,
                                         ErrorSummary::InvalidArgument,
                                         ErrorLevel::Usage); // 0xE0E01BF4
constexpr ResultCode ERR_INVALID_ADDRESS(ErrorDescription::InvalidAddress, ErrorModule::OS,
                                         ErrorSummary::InvalidArgument,
                                         ErrorLevel::Usage); // 0xE0E01BF5
constexpr ResultCode ERR_INVALID_ADDRESS_STATE(ErrorDescription::InvalidAddress, ErrorModule::OS,
                                               ErrorSummary::InvalidState,
                                               ErrorLevel::Usage); // 0xE0A01BF5
constexpr ResultCode ERR_INVALID_POINTER(ErrorDescription::InvalidPointer, ErrorModule::Kernel,
                                         ErrorSummary::InvalidArgument,
                                         ErrorLevel::Permanent); // 0xD8E007F6
constexpr ResultCode ERR_INVALID_HANDLE(ErrorDescription::InvalidHandle, ErrorModule::Kernel,
                                        ErrorSummary::InvalidArgument,
                                        ErrorLevel::Permanent); // 0xD8E007F7
/// Alternate code returned instead of ERR_INVALID_HANDLE in some code paths.
constexpr ResultCode ERR_INVALID_HANDLE_OS(ErrorDescription::InvalidHandle, ErrorModule::OS,
                                           ErrorSummary::WrongArgument,
                                           ErrorLevel::Permanent); // 0xD9001BF7
constexpr ResultCode ERR_NOT_FOUND(ErrorDescription::NotFound, ErrorModule::Kernel,
                                   ErrorSummary::NotFound, ErrorLevel::Permanent); // 0xD88007FA
constexpr ResultCode ERR_OUT_OF_RANGE(ErrorDescription::OutOfRange, ErrorModule::OS,
                                      ErrorSummary::InvalidArgument,
                                      ErrorLevel::Usage); // 0xE0E01BFD
constexpr ResultCode ERR_OUT_OF_RANGE_KERNEL(ErrorDescription::OutOfRange, ErrorModule::Kernel,
                                             ErrorSummary::InvalidArgument,
                                             ErrorLevel::Permanent); // 0xD8E007FD
constexpr ResultCode RESULT_TIMEOUT(ErrorDescription::Timeout, ErrorModule::OS,
                                    ErrorSummary::StatusChanged, ErrorLevel::Info);
/// Returned when Accept() is called on a port with no sessions to be accepted.
constexpr ResultCode ERR_NO_PENDING_SESSIONS(ErrCodes::NoPendingSessions, ErrorModule::OS,
                                             ErrorSummary::WouldBlock,
                                             ErrorLevel::Permanent); // 0xD8401823

} // namespace Kernel
