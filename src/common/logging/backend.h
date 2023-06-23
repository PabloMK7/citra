// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include "common/logging/filter.h"
#include "common/logging/log.h"

namespace FileUtil {
class IOFile;
}

namespace Common::Log {

/**
 * Interface for logging backends. As loggers can be created and removed at runtime, this can be
 * used by a frontend for adding a custom logging backend as needed
 */
class Backend {
public:
    virtual ~Backend() = default;

    virtual void SetFilter(const Filter& new_filter) {
        filter = new_filter;
    }
    virtual const char* GetName() const = 0;
    virtual void Write(const Entry& entry) = 0;

private:
    Filter filter;
};

/**
 * Backend that writes to stderr without any color commands
 */
class ConsoleBackend : public Backend {
public:
    ~ConsoleBackend() override;

    static const char* Name() {
        return "console";
    }
    const char* GetName() const override {
        return Name();
    }
    void Write(const Entry& entry) override;
};

/**
 * Backend that writes to stderr and with color
 */
class ColorConsoleBackend : public Backend {
public:
    ~ColorConsoleBackend() override;

    static const char* Name() {
        return "color_console";
    }

    const char* GetName() const override {
        return Name();
    }
    void Write(const Entry& entry) override;
};

/**
 * Backend that writes to the Android logcat
 */
class LogcatBackend : public Backend {
public:
    ~LogcatBackend() override;

    static const char* Name() {
        return "logcat";
    }

    const char* GetName() const override {
        return Name();
    }
    void Write(const Entry& entry) override;
};

/**
 * Backend that writes to a file passed into the constructor
 */
class FileBackend : public Backend {
public:
    ~FileBackend() override;

    explicit FileBackend(const std::string& filename);

    static const char* Name() {
        return "file";
    }

    const char* GetName() const override {
        return Name();
    }

    void Write(const Entry& entry) override;

private:
    std::unique_ptr<FileUtil::IOFile> file;
    std::size_t bytes_written = 0;
};

/**
 * Backend that writes to Visual Studio's output window
 */
class DebuggerBackend : public Backend {
public:
    ~DebuggerBackend() override;

    static const char* Name() {
        return "debugger";
    }
    const char* GetName() const override {
        return Name();
    }
    void Write(const Entry& entry) override;
};

void AddBackend(std::unique_ptr<Backend> backend);

void RemoveBackend(std::string_view backend_name);

Backend* GetBackend(std::string_view backend_name);

/**
 * The global filter will prevent any messages from even being processed if they are filtered. Each
 * backend can have a filter, but if the level is lower than the global filter, the backend will
 * never get the message
 */
void SetGlobalFilter(const Filter& filter);
} // namespace Common::Log
