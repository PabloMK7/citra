// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/result.h"

namespace FileSys {

const ResultCode ERROR_INVALID_PATH(ErrorDescription::FS_InvalidPath, ErrorModule::FS,
                                    ErrorSummary::InvalidArgument, ErrorLevel::Usage);
const ResultCode ERROR_UNSUPPORTED_OPEN_FLAGS(ErrorDescription::FS_UnsupportedOpenFlags,
                                              ErrorModule::FS, ErrorSummary::NotSupported,
                                              ErrorLevel::Usage);
const ResultCode ERROR_INVALID_OPEN_FLAGS(ErrorDescription::FS_InvalidOpenFlags, ErrorModule::FS,
                                          ErrorSummary::Canceled, ErrorLevel::Status);
const ResultCode ERROR_INVALID_READ_FLAG(ErrorDescription::FS_InvalidReadFlag, ErrorModule::FS,
                                         ErrorSummary::InvalidArgument, ErrorLevel::Usage);
const ResultCode ERROR_FILE_NOT_FOUND(ErrorDescription::FS_FileNotFound, ErrorModule::FS,
                                      ErrorSummary::NotFound, ErrorLevel::Status);
const ResultCode ERROR_PATH_NOT_FOUND(ErrorDescription::FS_PathNotFound, ErrorModule::FS,
                                      ErrorSummary::NotFound, ErrorLevel::Status);
const ResultCode ERROR_NOT_FOUND(ErrorDescription::FS_NotFound, ErrorModule::FS,
                                 ErrorSummary::NotFound, ErrorLevel::Status);
const ResultCode ERROR_UNEXPECTED_FILE_OR_DIRECTORY(ErrorDescription::FS_UnexpectedFileOrDirectory,
                                                    ErrorModule::FS, ErrorSummary::NotSupported,
                                                    ErrorLevel::Usage);
const ResultCode ERROR_UNEXPECTED_FILE_OR_DIRECTORY_SDMC(ErrorDescription::FS_NotAFile,
                                                         ErrorModule::FS, ErrorSummary::Canceled,
                                                         ErrorLevel::Status);
const ResultCode ERROR_DIRECTORY_ALREADY_EXISTS(ErrorDescription::FS_DirectoryAlreadyExists,
                                                ErrorModule::FS, ErrorSummary::NothingHappened,
                                                ErrorLevel::Status);
const ResultCode ERROR_FILE_ALREADY_EXISTS(ErrorDescription::FS_FileAlreadyExists, ErrorModule::FS,
                                           ErrorSummary::NothingHappened, ErrorLevel::Status);
const ResultCode ERROR_ALREADY_EXISTS(ErrorDescription::FS_AlreadyExists, ErrorModule::FS,
                                      ErrorSummary::NothingHappened, ErrorLevel::Status);
const ResultCode ERROR_DIRECTORY_NOT_EMPTY(ErrorDescription::FS_DirectoryNotEmpty, ErrorModule::FS,
                                           ErrorSummary::Canceled, ErrorLevel::Status);
const ResultCode ERROR_GAMECARD_NOT_INSERTED(ErrorDescription::FS_GameCardNotInserted,
                                             ErrorModule::FS, ErrorSummary::NotFound,
                                             ErrorLevel::Status);
const ResultCode ERROR_INCORRECT_EXEFS_READ_SIZE(ErrorDescription::FS_IncorrectExeFSReadSize,
                                                 ErrorModule::FS, ErrorSummary::NotSupported,
                                                 ErrorLevel::Usage);
const ResultCode ERROR_ROMFS_NOT_FOUND(ErrorDescription::FS_RomFSNotFound, ErrorModule::FS,
                                       ErrorSummary::NotFound, ErrorLevel::Status);
const ResultCode ERROR_COMMAND_NOT_ALLOWED(ErrorDescription::FS_CommandNotAllowed, ErrorModule::FS,
                                           ErrorSummary::WrongArgument, ErrorLevel::Permanent);
const ResultCode ERROR_EXEFS_SECTION_NOT_FOUND(ErrorDescription::FS_ExeFSSectionNotFound,
                                               ErrorModule::FS, ErrorSummary::NotFound,
                                               ErrorLevel::Status);

} // namespace FileSys
