/**
* Copyright (C) 2005-2013 Gekko Emulator
*
* @file    file_utils.h
* @author  ShizZy <shizzy247@gmail.com>
* @date    2013-01-27
* @brief   Crossplatform file utility functions
* @remark  Borrowed from Dolphin Emulator
*
* @section LICENSE
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details at
* http://www.gnu.org/copyleft/gpl.html
*
* Official project repository can be found at:
* http://code.google.com/p/gekko-gc-emu/
*/

#ifndef COMMON_FILE_UTILS_H_
#define COMMON_FILE_UTILS_H_

#include <fstream>
#include <cstdio>
#include <string>
#include <vector>
#include <string.h>

#include "common.h"

namespace common {

// Returns true if file filename exists
bool FileExists(const std::string &filename);

// Returns true if filename is a directory
bool IsDirectory(const std::string &filename);

// Returns the size of filename (64bit)
u64 GetFileSize(const std::string &filename);

// Overloaded GetSize, accepts file descriptor
u64 GetFileSize(const int fd);

// Overloaded GetSize, accepts FILE*
u64 GetFileSize(FILE *f);

// Returns true if successful, or path already exists.
bool CreateDir(const std::string &filename);

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string &fullPath);

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool DeleteFile(const std::string &filename);

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string &filename);

// renames file srcFilename to destFilename, returns true on success 
bool RenameFile(const std::string &srcFilename, const std::string &destFilename);

// copies file srcFilename to destFilename, returns true on success 
bool CopyFile(const std::string &srcFilename, const std::string &destFilename);

// creates an empty file filename, returns true on success 
bool CreateEmptyFile(const std::string &filename);

// deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string &directory);

// Returns the current directory
std::string GetCurrentDir();

// Set the current directory to given directory
bool SetCurrentDir(const std::string &directory);

bool WriteStringToFile(bool text_file, const std::string &str, const char *filename);
bool ReadFileToString(bool text_file, const char *filename, std::string &str);

}  // namespace

#endif // COMMON_FILE_UTILS_H_
