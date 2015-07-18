// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <fstream>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "common/common_types.h"

// User directory indices for GetUserPath
enum {
    D_USER_IDX,
    D_ROOT_IDX,
    D_CONFIG_IDX,
    D_GAMECONFIG_IDX,
    D_MAPS_IDX,
    D_CACHE_IDX,
    D_SHADERCACHE_IDX,
    D_SHADERS_IDX,
    D_STATESAVES_IDX,
    D_SCREENSHOTS_IDX,
    D_SDMC_IDX,
    D_NAND_IDX,
    D_SYSDATA_IDX,
    D_HIRESTEXTURES_IDX,
    D_DUMP_IDX,
    D_DUMPFRAMES_IDX,
    D_DUMPAUDIO_IDX,
    D_DUMPTEXTURES_IDX,
    D_DUMPDSP_IDX,
    D_LOGS_IDX,
    D_SYSCONF_IDX,
    F_EMUCONFIG_IDX,
    F_DEBUGGERCONFIG_IDX,
    F_LOGGERCONFIG_IDX,
    F_MAINLOG_IDX,
    F_RAMDUMP_IDX,
    F_ARAMDUMP_IDX,
    F_SYSCONF_IDX,
    NUM_PATH_INDICES
};

namespace FileUtil
{

// FileSystem tree node/
struct FSTEntry
{
    bool isDirectory;
    u64 size;                       // file length or number of entries from children
    std::string physicalName;       // name on disk
    std::string virtualName;        // name in FST names table
    std::vector<FSTEntry> children;
};

// Returns true if file filename exists
bool Exists(const std::string &filename);

// Returns true if filename is a directory
bool IsDirectory(const std::string &filename);

// Returns the size of filename (64bit)
u64 GetSize(const std::string &filename);

// Overloaded GetSize, accepts file descriptor
u64 GetSize(const int fd);

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE *f);

// Returns true if successful, or path already exists.
bool CreateDir(const std::string &filename);

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string &fullPath);

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string &filename);

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string &filename);

// renames file srcFilename to destFilename, returns true on success
bool Rename(const std::string &srcFilename, const std::string &destFilename);

// copies file srcFilename to destFilename, returns true on success
bool Copy(const std::string &srcFilename, const std::string &destFilename);

// creates an empty file filename, returns true on success
bool CreateEmptyFile(const std::string &filename);

// Scans the directory tree gets, starting from _Directory and adds the
// results into parentEntry. Returns the number of files+directories found
u32 ScanDirectoryTree(const std::string &directory, FSTEntry& parentEntry);

// deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string &directory);

// Returns the current directory
std::string GetCurrentDir();

// Create directory and copy contents (does not overwrite existing files)
void CopyDir(const std::string &source_path, const std::string &dest_path);

// Set the current directory to given directory
bool SetCurrentDir(const std::string &directory);

// Returns a pointer to a string with a Citra data dir in the user's home
// directory. To be used in "multi-user" mode (that is, installed).
const std::string& GetUserPath(const unsigned int DirIDX, const std::string &newPath="");

// Returns the path to where the sys file are
std::string GetSysDirectory();

#ifdef __APPLE__
std::string GetBundleDirectory();
#endif

#ifdef _WIN32
std::string &GetExeDirectory();
#endif

size_t WriteStringToFile(bool text_file, const std::string &str, const char *filename);
size_t ReadFileToString(bool text_file, const char *filename, std::string &str);

/**
 * Splits the filename into 8.3 format
 * Loosely implemented following https://en.wikipedia.org/wiki/8.3_filename
 * @param filename The normal filename to use
 * @param short_name A 9-char array in which the short name will be written
 * @param extension A 4-char array in which the extension will be written
 */
void SplitFilename83(const std::string& filename, std::array<char, 9>& short_name,
                     std::array<char, 4>& extension);

// simple wrapper for cstdlib file functions to
// hopefully will make error checking easier
// and make forgetting an fclose() harder
class IOFile : public NonCopyable
{
public:
    IOFile();
    IOFile(std::FILE* file);
    IOFile(const std::string& filename, const char openmode[]);

    ~IOFile();

    IOFile(IOFile&& other);
    IOFile& operator=(IOFile&& other);

    void Swap(IOFile& other);

    bool Open(const std::string& filename, const char openmode[]);
    bool Close();

    template <typename T>
    size_t ReadArray(T* data, size_t length)
    {
        if (!IsOpen()) {
            m_good = false;
            return -1;
        }

        size_t items_read = std::fread(data, sizeof(T), length, m_file);
        if (items_read != length)
            m_good = false;

        return items_read;
    }

    template <typename T>
    size_t WriteArray(const T* data, size_t length)
    {
        static_assert(std::is_standard_layout<T>::value, "Given array does not consist of standard layout objects");
        // TODO: gcc 4.8 does not support is_trivially_copyable, but we really should check for it here.
        //static_assert(std::is_trivially_copyable<T>::value, "Given array does not consist of trivially copyable objects");

        if (!IsOpen()) {
            m_good = false;
            return -1;
        }

        size_t items_written = std::fwrite(data, sizeof(T), length, m_file);
        if (items_written != length)
            m_good = false;

        return items_written;
    }

    size_t ReadBytes(void* data, size_t length)
    {
        return ReadArray(reinterpret_cast<char*>(data), length);
    }

    size_t WriteBytes(const void* data, size_t length)
    {
        return WriteArray(reinterpret_cast<const char*>(data), length);
    }

    template<typename T>
    size_t WriteObject(const T& object) {
        static_assert(!std::is_pointer<T>::value, "Given object is a pointer");
        return WriteArray(&object, 1);
    }

    bool IsOpen() { return nullptr != m_file; }

    // m_good is set to false when a read, write or other function fails
    bool IsGood() {    return m_good; }
    operator void*() { return m_good ? m_file : nullptr; }

    std::FILE* ReleaseHandle();

    std::FILE* GetHandle() { return m_file; }

    void SetHandle(std::FILE* file);

    bool Seek(s64 off, int origin);
    u64 Tell();
    u64 GetSize();
    bool Resize(u64 size);
    bool Flush();

    // clear error state
    void Clear() { m_good = true; std::clearerr(m_file); }

    std::FILE* m_file;
    bool m_good;
private:
    IOFile(IOFile&);
    IOFile& operator=(IOFile& other);
};

}  // namespace

// To deal with Windows being dumb at unicode:
template <typename T>
void OpenFStream(T& fstream, const std::string& filename, std::ios_base::openmode openmode)
{
#ifdef _WIN32
    fstream.open(Common::UTF8ToTStr(filename).c_str(), openmode);
#else
    fstream.open(filename.c_str(), openmode);
#endif
}
