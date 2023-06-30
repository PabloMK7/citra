// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace Service::NFC {

namespace ErrCodes {
enum {
    InvalidArgumentValue = 80,
    InvalidArgument = 81,

    InvalidChecksum = 200,
    WriteFailed = 328,

    CommandInvalidForState = 512,
    NotAnAmiibo = 522,
    CorruptedData = 536,
    AppDataUninitialized = 544,
    RegistrationUnitialized = 552,
    ApplicationAreaExist = 560,
    AppIdMismatch = 568,

    CommunicationLost = 608,
    NoAdapterDetected = 616,
};
} // namespace ErrCodes

constexpr ResultCode ResultInvalidArgumentValue(ErrCodes::InvalidArgumentValue, ErrorModule::NFC,
                                                ErrorSummary::InvalidArgument, ErrorLevel::Status);
constexpr ResultCode ResultInvalidArgument(ErrCodes::InvalidArgument, ErrorModule::NFC,
                                           ErrorSummary::InvalidArgument, ErrorLevel::Status);
constexpr ResultCode ResultCommandInvalidForState(ErrCodes::CommandInvalidForState,
                                                  ErrorModule::NFC, ErrorSummary::InvalidState,
                                                  ErrorLevel::Status);
constexpr ResultCode ResultNotAnAmiibo(ErrCodes::NotAnAmiibo, ErrorModule::NFC,
                                       ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr ResultCode ResultCorruptedData(ErrCodes::CorruptedData, ErrorModule::NFC,
                                         ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr ResultCode ResultWriteAmiiboFailed(ErrCodes::WriteFailed, ErrorModule::NFC,
                                             ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr ResultCode ResultApplicationAreaIsNotInitialized(ErrCodes::AppDataUninitialized,
                                                           ErrorModule::NFC,
                                                           ErrorSummary::InvalidState,
                                                           ErrorLevel::Status);
constexpr ResultCode ResultRegistrationIsNotInitialized(ErrCodes::RegistrationUnitialized,
                                                        ErrorModule::NFC,
                                                        ErrorSummary::InvalidState,
                                                        ErrorLevel::Status);
constexpr ResultCode ResultApplicationAreaExist(ErrCodes::ApplicationAreaExist, ErrorModule::NFC,
                                                ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr ResultCode ResultWrongApplicationAreaId(ErrCodes::AppIdMismatch, ErrorModule::NFC,
                                                  ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr ResultCode ResultCommunicationLost(ErrCodes::CommunicationLost, ErrorModule::NFC,
                                             ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr ResultCode ResultNoAdapterDetected(ErrCodes::NoAdapterDetected, ErrorModule::NFC,
                                             ErrorSummary::InvalidState, ErrorLevel::Status);

} // namespace Service::NFC
