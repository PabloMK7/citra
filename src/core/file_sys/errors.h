// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"

namespace FileSys {

namespace ErrCodes {
enum {
    RomFSNotFound = 100,
    ArchiveNotMounted = 101,
    FileNotFound = 112,
    PathNotFound = 113,
    GameCardNotInserted = 141,
    NotFound = 120,
    FileAlreadyExists = 180,
    DirectoryAlreadyExists = 185,
    AlreadyExists = 190,
    InsufficientSpace = 210,
    InvalidOpenFlags = 230,
    DirectoryNotEmpty = 240,
    NotAFile = 250,
    NotFormatted = 340, ///< This is used by the FS service when creating a SaveData archive
    ExeFSSectionNotFound = 567,
    CommandNotAllowed = 630,
    InvalidReadFlag = 700,
    InvalidPath = 702,
    WriteBeyondEnd = 705,
    UnsupportedOpenFlags = 760,
    IncorrectExeFSReadSize = 761,
    UnexpectedFileOrDirectory = 770,
};
}

constexpr ResultCode ERROR_INVALID_PATH(ErrCodes::InvalidPath, ErrorModule::FS,
                                        ErrorSummary::InvalidArgument, ErrorLevel::Usage);
constexpr ResultCode ERROR_UNSUPPORTED_OPEN_FLAGS(ErrCodes::UnsupportedOpenFlags, ErrorModule::FS,
                                                  ErrorSummary::NotSupported, ErrorLevel::Usage);
constexpr ResultCode ERROR_INVALID_OPEN_FLAGS(ErrCodes::InvalidOpenFlags, ErrorModule::FS,
                                              ErrorSummary::Canceled, ErrorLevel::Status);
constexpr ResultCode ERROR_INVALID_READ_FLAG(ErrCodes::InvalidReadFlag, ErrorModule::FS,
                                             ErrorSummary::InvalidArgument, ErrorLevel::Usage);
constexpr ResultCode ERROR_FILE_NOT_FOUND(ErrCodes::FileNotFound, ErrorModule::FS,
                                          ErrorSummary::NotFound, ErrorLevel::Status);
constexpr ResultCode ERROR_PATH_NOT_FOUND(ErrCodes::PathNotFound, ErrorModule::FS,
                                          ErrorSummary::NotFound, ErrorLevel::Status);
constexpr ResultCode ERROR_NOT_FOUND(ErrCodes::NotFound, ErrorModule::FS, ErrorSummary::NotFound,
                                     ErrorLevel::Status);
constexpr ResultCode ERROR_UNEXPECTED_FILE_OR_DIRECTORY(ErrCodes::UnexpectedFileOrDirectory,
                                                        ErrorModule::FS, ErrorSummary::NotSupported,
                                                        ErrorLevel::Usage);
constexpr ResultCode ERROR_UNEXPECTED_FILE_OR_DIRECTORY_SDMC(ErrCodes::NotAFile, ErrorModule::FS,
                                                             ErrorSummary::Canceled,
                                                             ErrorLevel::Status);
constexpr ResultCode ERROR_DIRECTORY_ALREADY_EXISTS(ErrCodes::DirectoryAlreadyExists,
                                                    ErrorModule::FS, ErrorSummary::NothingHappened,
                                                    ErrorLevel::Status);
constexpr ResultCode ERROR_FILE_ALREADY_EXISTS(ErrCodes::FileAlreadyExists, ErrorModule::FS,
                                               ErrorSummary::NothingHappened, ErrorLevel::Status);
constexpr ResultCode ERROR_ALREADY_EXISTS(ErrCodes::AlreadyExists, ErrorModule::FS,
                                          ErrorSummary::NothingHappened, ErrorLevel::Status);
constexpr ResultCode ERROR_DIRECTORY_NOT_EMPTY(ErrCodes::DirectoryNotEmpty, ErrorModule::FS,
                                               ErrorSummary::Canceled, ErrorLevel::Status);
constexpr ResultCode ERROR_GAMECARD_NOT_INSERTED(ErrCodes::GameCardNotInserted, ErrorModule::FS,
                                                 ErrorSummary::NotFound, ErrorLevel::Status);
constexpr ResultCode ERROR_INCORRECT_EXEFS_READ_SIZE(ErrCodes::IncorrectExeFSReadSize,
                                                     ErrorModule::FS, ErrorSummary::NotSupported,
                                                     ErrorLevel::Usage);
constexpr ResultCode ERROR_ROMFS_NOT_FOUND(ErrCodes::RomFSNotFound, ErrorModule::FS,
                                           ErrorSummary::NotFound, ErrorLevel::Status);
constexpr ResultCode ERROR_COMMAND_NOT_ALLOWED(ErrCodes::CommandNotAllowed, ErrorModule::FS,
                                               ErrorSummary::WrongArgument, ErrorLevel::Permanent);
constexpr ResultCode ERROR_EXEFS_SECTION_NOT_FOUND(ErrCodes::ExeFSSectionNotFound, ErrorModule::FS,
                                                   ErrorSummary::NotFound, ErrorLevel::Status);
constexpr ResultCode ERROR_INSUFFICIENT_SPACE(ErrCodes::InsufficientSpace, ErrorModule::FS,
                                              ErrorSummary::OutOfResource, ErrorLevel::Status);

/// Returned when a function is passed an invalid archive handle.
constexpr ResultCode ERR_INVALID_ARCHIVE_HANDLE(ErrCodes::ArchiveNotMounted, ErrorModule::FS,
                                                ErrorSummary::NotFound,
                                                ErrorLevel::Status); // 0xC8804465
constexpr ResultCode ERR_WRITE_BEYOND_END(ErrCodes::WriteBeyondEnd, ErrorModule::FS,
                                          ErrorSummary::InvalidArgument, ErrorLevel::Usage);

/**
 * Variant of ERROR_NOT_FOUND returned in some places in the code. Unknown if these usages are
 * correct or a bug.
 */
constexpr ResultCode ERR_NOT_FOUND_INVALID_STATE(ErrCodes::NotFound, ErrorModule::FS,
                                                 ErrorSummary::InvalidState, ErrorLevel::Status);
constexpr ResultCode ERR_NOT_FORMATTED(ErrCodes::NotFormatted, ErrorModule::FS,
                                       ErrorSummary::InvalidState, ErrorLevel::Status);

} // namespace FileSys
