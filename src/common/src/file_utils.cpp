/**
* Copyright (C) 2005-2013 Gekko Emulator
*
* @file    file_utils.cpp
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

#include "types.h"
#include "file_utils.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>		// for SHGetFolderPath
#include <shellapi.h>
#include <commdlg.h>	// for GetSaveFileName
#include <io.h>
#include <direct.h>		// getcwd
#else
#include <sys/param.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFBundle.h>
#endif

#include <fstream>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(m)  (((m)&S_IFMT) == S_IFDIR)
#endif

#ifdef BSD4_4
#define stat64 stat
#define fstat64 fstat
#endif

#ifdef _MSC_VER
#define __strdup _strdup
#define __getcwd _getcwd
#define __chdir _chdir

#define fseeko _fseeki64
#define ftello _ftelli64
#define atoll _atoi64
#define stat64 _stat64
#define fstat64 _fstat64
#define fileno _fileno

#else
#define __strdup strdup
#define __getcwd getcwd
#define __chdir chdir
#endif

namespace common {

// Remove any ending forward slashes from directory paths
// Modifies argument.
static void StripTailDirSlashes(std::string &fname) {
    if (fname.length() > 1)  {
        size_t i = fname.length() - 1;
        while (fname[i] == '/') {
            fname[i--] = '\0';
        }
    }
}

// Returns true if file filename exists
bool FileExists(const std::string &filename) {
    struct stat64 file_info;
    std::string copy(filename);
    StripTailDirSlashes(copy);
    return (stat64(copy.c_str(), &file_info) == 0);
}

// Returns true if filename is a directory
bool IsDirectory(const std::string &filename) {
    struct stat64 file_info;
    std::string copy(filename);
    StripTailDirSlashes(copy);
    if (stat64(copy.c_str(), &file_info) < 0) {
        LOG_WARNING(TCOMMON, "IsDirectory: stat failed on %s", filename.c_str());
        return false;
    }
    return S_ISDIR(file_info.st_mode);
}

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool DeleteFile(const std::string &filename) {
    LOG_INFO(TCOMMON, "Delete: file %s", filename.c_str());
    // Return true because we care about the file no 
    // being there, not the actual delete.
    if (!FileExists(filename)) {
        LOG_WARNING(TCOMMON, "Delete: %s does not exists", filename.c_str());
        return true;
    }
    // We can't delete a directory
    if (IsDirectory(filename)) {
        LOG_WARNING(TCOMMON, "Delete failed: %s is a directory", filename.c_str());
        return false;
    }
#ifdef _WIN32
    if (!DeleteFile(filename.c_str())) {
        LOG_WARNING(TCOMMON, "Delete: DeleteFile failed on %s", filename.c_str());
        return false;
    }
#else
    if (unlink(filename.c_str()) == -1) {
        LOG_WARNING(TCOMMON, "Delete: unlink failed on %s", filename.c_str());
        return false;
    }
#endif
    return true;
}

// Returns true if successful, or path already exists.
bool CreateDir(const std::string &path) {
    LOG_INFO(TCOMMON, "CreateDir: directory %s", path.c_str());
#ifdef _WIN32
    if (::CreateDirectory(path.c_str(), NULL)) {
        return true;
    }
    DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS)
    {
        LOG_WARNING(TCOMMON, "CreateDir: CreateDirectory failed on %s: already exists", path.c_str());
        return true;
    }
    LOG_ERROR(TCOMMON, "CreateDir: CreateDirectory failed on %s: %i", path.c_str(), error);
    return false;
#else
    if (mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
    int err = errno;
    if (err == EEXIST) {
        LOG_WARNING(TCOMMON, "CreateDir: mkdir failed on %s: already exists", path.c_str());
        return true;
    }
    LOG_ERROR(TCOMMON, "CreateDir: mkdir failed on %s: %s", path.c_str(), strerror(err));
    return false;
#endif
}

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string &fullPath) {
    int panicCounter = 100;
    LOG_INFO(TCOMMON, "CreateFullPath: path %s", fullPath.c_str());

    if (FileExists(fullPath)) {
        LOG_INFO(TCOMMON, "CreateFullPath: path exists %s", fullPath.c_str());
        return true;
    }

    size_t position = 0;
    while (1) {
        // Find next sub path
        position = fullPath.find('/', position);

        // we're done, yay!
        if (position == fullPath.npos) {
            return true;
        }
        std::string subPath = fullPath.substr(0, position);
        if (!IsDirectory(subPath)) CreateDir(subPath);

        // A safety check
        panicCounter--;
        if (panicCounter <= 0) {
            LOG_ERROR(TCOMMON, "CreateFullPath: directory structure too deep");
            return false;
        }
        position++;
    }
}

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string &filename) {
    LOG_INFO(TCOMMON, "DeleteDir: directory %s", filename.c_str());
    // check if a directory
    if (!IsDirectory(filename)) {
        LOG_ERROR(TCOMMON, "DeleteDir: Not a directory %s", filename.c_str());
        return false;
    }
#ifdef _WIN32
    if (::RemoveDirectory(filename.c_str()))
        return true;
#else
    if (rmdir(filename.c_str()) == 0)
        return true;
#endif
    LOG_ERROR(TCOMMON, "DeleteDir: %s", filename.c_str());
    return false;
}

// renames file srcFilename to destFilename, returns true on success 
bool RenameFile(const std::string &srcFilename, const std::string &destFilename) {
    LOG_INFO(TCOMMON, "Rename: %s --> %s", 
        srcFilename.c_str(), destFilename.c_str());
    if (rename(srcFilename.c_str(), destFilename.c_str()) == 0)
        return true;
    LOG_ERROR(TCOMMON, "Rename: failed %s --> %s", srcFilename.c_str(), destFilename.c_str());
    return false;
}

// copies file srcFilename to destFilename, returns true on success 
bool CopyFile(const std::string &srcFilename, const std::string &destFilename) {
    LOG_INFO(TCOMMON, "Copy: %s --> %s", 
        srcFilename.c_str(), destFilename.c_str());
#ifdef _WIN32
    if (::CopyFile(srcFilename.c_str(), destFilename.c_str(), FALSE))
        return true;

    LOG_ERROR(TCOMMON, "Copy: failed %s --> %s", srcFilename.c_str(), destFilename.c_str());
    return false;
#else
    char buffer[1024];

    // Open input file
    FILE *input = fopen(srcFilename.c_str(), "rb");
    if (!input) {
        LOG_ERROR(TCOMMON, "Copy: input failed %s --> %s", srcFilename.c_str(), 
            destFilename.c_str());
        return false;
    }
    // open output file
    FILE *output = fopen(destFilename.c_str(), "wb");
    if (!output) {
        fclose(input);
        LOG_ERROR(TCOMMON, "Copy: output failed %s --> %s", srcFilename.c_str(), 
            destFilename.c_str());
        return false;
    }
    // copy loop
    while (!feof(input)) {
        // read input
        int rnum = fread(buffer, sizeof(char), 1024, input);
        if (rnum != 1024) {
            if (ferror(input) != 0) {
                LOG_ERROR(TCOMMON, "Copy: failed reading from source, %s --> %s", 
                    srcFilename.c_str(), destFilename.c_str());
                goto bail;
            }
        }
        // write output
        int wnum = fwrite(buffer, sizeof(char), rnum, output);
        if (wnum != rnum) {
            LOG_ERROR(TCOMMON, "Copy: failed writing to output, %s --> %s", 
                srcFilename.c_str(), destFilename.c_str());
            goto bail;
        }
    }
    // close flushs
    fclose(input);
    fclose(output);
    return true;
bail:
    if (input)
        fclose(input);
    if (output)
        fclose(output);
    return false;
#endif
}

// Returns the size of filename (64bit)
u64 GetFileSize(const std::string &filename) {
    if (!FileExists(filename)) {
        LOG_WARNING(TCOMMON, "GetSize: failed %s: No such file", filename.c_str());
        return 0;
    }
    if (IsDirectory(filename)) {
        LOG_WARNING(TCOMMON, "GetSize: failed %s: is a directory", filename.c_str());
        return 0;
    }
    struct stat64 buf;
    if (stat64(filename.c_str(), &buf) == 0) {
        LOG_DEBUG(TCOMMON, "GetSize: %s: %lld", filename.c_str(), (long long)buf.st_size);
        return buf.st_size;
    }
    LOG_ERROR(TCOMMON, "GetSize: Stat failed %s", filename.c_str());
    return 0;
}

// Overloaded GetSize, accepts file descriptor
u64 GetFileSize(const int fd) {
	struct stat64 buf;
	if (fstat64(fd, &buf) != 0) {
		LOG_ERROR(TCOMMON, "GetSize: stat failed %i", fd);
		return 0;
	}
	return buf.st_size;
}

// Overloaded GetSize, accepts FILE*
u64 GetFileSize(FILE *f) {
	// can't use off_t here because it can be 32-bit
	u64 pos = ftello(f);
	if (fseeko(f, 0, SEEK_END) != 0) {
		LOG_ERROR(TCOMMON, "GetSize: seek failed %p", f);
		return 0;
	}
	u64 size = ftello(f);
	if ((size != pos) && (fseeko(f, pos, SEEK_SET) != 0)) {
		LOG_ERROR(TCOMMON, "GetSize: seek failed %p", f);
		return 0;
	}
	return size;
}

// creates an empty file filename, returns true on success 
bool CreateEmptyFile(const std::string &filename) {
    LOG_INFO(TCOMMON, "CreateEmptyFile: %s", filename.c_str()); 

    FILE *pFile = fopen(filename.c_str(), "wb");
    if (!pFile) {
        LOG_ERROR(TCOMMON, "CreateEmptyFile: failed %s", filename.c_str());
        return false;
    }
    fclose(pFile);
    return true;
}

// Deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string &directory) {
    LOG_INFO(TCOMMON, "DeleteDirRecursively: %s", directory.c_str());
#ifdef _WIN32
    // Find the first file in the directory.
    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile((directory + "\\*").c_str(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE) {
        FindClose(hFind);
        return false;
    }

    // windows loop
    do {
        const std::string virtualName = ffd.cFileName;
#else
    struct dirent dirent, *result = NULL;
    DIR *dirp = opendir(directory.c_str());
    if (!dirp) {
        return false;
    }
    // non windows loop
    while (!readdir_r(dirp, &dirent, &result) && result) {
        const std::string virtualName = result->d_name;
#endif
        // check for "." and ".."
        if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
            ((virtualName[0] == '.') && (virtualName[1] == '.') && 
            (virtualName[2] == '\0'))) {
            continue;
        }
        std::string newPath = directory + '/' + virtualName;
        if (IsDirectory(newPath)) {
            if (!DeleteDirRecursively(newPath))
                return false;
        } else {
            if (!DeleteFile(newPath))
                return false;
        }

#ifdef _WIN32
    } while (FindNextFile(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    }
    closedir(dirp);
#endif
    DeleteDir(directory);

    return true;
}

// Returns the current directory
std::string GetCurrentDir() {
    char *dir;
    // Get the current working directory (getcwd uses malloc) 
    if (!(dir = __getcwd(NULL, 0))) {

        LOG_ERROR(TCOMMON, "GetCurrentDirectory failed:");
        return NULL;
    }
    std::string strDir = dir;
    free(dir);
    return strDir;
}

// Sets the current directory to the given directory
bool SetCurrentDir(const std::string &directory) {
    return __chdir(directory.c_str()) == 0;
}

bool WriteStringToFile(bool text_file, const std::string &str, const char *filename) {
    FILE *f = fopen(filename, text_file ? "w" : "wb");
    if (!f) {
        return false;
    }
    size_t len = str.size();
    if (len != fwrite(str.data(), 1, str.size(), f)) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

bool ReadFileToString(bool text_file, const char *filename, std::string &str) {
    FILE *f = fopen(filename, text_file ? "r" : "rb");
    if (!f) {
        return false;
    }
    size_t len = (size_t)GetFileSize(f);
    char *buf = new char[len + 1];
    buf[fread(buf, 1, len, f)] = 0;
    str = std::string(buf, len);
    fclose(f);
    delete [] buf;
    return true;
}

} // namespace
