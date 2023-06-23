// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <thread>
#include <vector>
#include "common/common_paths.h"
#ifdef _WIN32
#include <share.h>   // For _SH_DENYWR
#include <windows.h> // For OutputDebugStringW
#else
#define _SH_DENYWR 0
#endif
#include "common/file_util.h"
#include "common/literals.h"
#include "common/settings.h"
#include "common/thread.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/logging/text_formatter.h"
#include "common/string_util.h"
#include "common/threadsafe_queue.h"

namespace Common::Log {

namespace {

/**
 * Interface for logging backends.
 */
class Backend {
public:
    virtual ~Backend() = default;

    virtual void Write(const Entry& entry) = 0;

    virtual void EnableForStacktrace() = 0;

    virtual void Flush() = 0;
};

/**
 * Backend that writes to stderr and with color
 */
class ColorConsoleBackend final : public Backend {
public:
    explicit ColorConsoleBackend() = default;

    ~ColorConsoleBackend() override = default;

    void Write(const Entry& entry) override {
        if (enabled.load(std::memory_order_relaxed)) {
            PrintColoredMessage(entry);
        }
    }

    void Flush() override {
        // stderr shouldn't be buffered
    }

    void EnableForStacktrace() override {
        enabled = true;
    }

    void SetEnabled(bool enabled_) {
        enabled = enabled_;
    }

private:
    std::atomic_bool enabled{false};
};

/**
 * Backend that writes to a file passed into the constructor
 */
class FileBackend final : public Backend {
public:
    explicit FileBackend(const std::string& filename) {
        auto old_filename = filename;
        old_filename += ".old.txt";

        // Existence checks are done within the functions themselves.
        // We don't particularly care if these succeed or not.
        static_cast<void>(FileUtil::Delete(old_filename));
        static_cast<void>(FileUtil::Rename(filename, old_filename));

        // _SH_DENYWR allows read only access to the file for other programs.
        // It is #defined to 0 on other platforms
        file = std::make_unique<FileUtil::IOFile>(filename, "w", _SH_DENYWR);
    }

    ~FileBackend() override = default;

    void Write(const Entry& entry) override {
        if (!enabled) {
            return;
        }

        bytes_written += file->WriteString(FormatLogMessage(entry).append(1, '\n'));

        using namespace Common::Literals;
        // Prevent logs from exceeding a set maximum size in the event that log entries are spammed.
        const auto write_limit = 100_MiB;
        const bool write_limit_exceeded = bytes_written > write_limit;
        if (entry.log_level >= Level::Error || write_limit_exceeded) {
            if (write_limit_exceeded) {
                // Stop writing after the write limit is exceeded.
                // Don't close the file so we can print a stacktrace if necessary
                enabled = false;
            }
            file->Flush();
        }
    }

    void Flush() override {
        file->Flush();
    }

    void EnableForStacktrace() override {
        enabled = true;
        bytes_written = 0;
    }

private:
    std::unique_ptr<FileUtil::IOFile> file;
    bool enabled = true;
    std::size_t bytes_written = 0;
};

/**
 * Backend that writes to Visual Studio's output window
 */
class DebuggerBackend final : public Backend {
public:
    explicit DebuggerBackend() = default;

    ~DebuggerBackend() override = default;

    void Write(const Entry& entry) override {
#ifdef _WIN32
        ::OutputDebugStringW(UTF8ToUTF16W(FormatLogMessage(entry).append(1, '\n')).c_str());
#endif
    }

    void Flush() override {}

    void EnableForStacktrace() override {}
};

#ifdef ANDROID
/**
 * Backend that writes to the Android logcat
 */
class LogcatBackend : public Backend {
public:
    explicit LogcatBackend() = default;

    ~LogcatBackend() override = default;

    void Write(const Entry& entry) override {
        PrintMessageToLogcat(entry);
    }

    void Flush() override {}

    void EnableForStacktrace() override {}
};
#endif

bool initialization_in_progress_suppress_logging = false;

/**
 * Static state as a singleton.
 */
class Impl {
public:
    static Impl& Instance() {
        if (!instance) {
            abort();
        }
        return *instance;
    }

    static void Initialize() {
        if (instance) {
            abort();
        }
        initialization_in_progress_suppress_logging = true;
        const auto& log_dir = FileUtil::GetUserPath(FileUtil::UserPath::LogDir);
        void(FileUtil::CreateDir(log_dir));
        Filter filter;
        filter.ParseFilterString(Settings::values.log_filter.GetValue());
        instance = std::unique_ptr<Impl, decltype(&Deleter)>(new Impl(log_dir + LOG_FILE, filter),
                                                             Deleter);
        initialization_in_progress_suppress_logging = false;
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void SetGlobalFilter(const Filter& f) {
        filter = f;
    }

    void SetColorConsoleBackendEnabled(bool enabled) {
        color_console_backend.SetEnabled(enabled);
    }

    void PushEntry(Class log_class, Level log_level, const char* filename, unsigned int line_num,
                   const char* function, std::string message) {
        if (!filter.CheckMessage(log_class, log_level))
            return;
        const Entry& entry =
            CreateEntry(log_class, log_level, filename, line_num, function, std::move(message));
        message_queue.Push(entry);
    }

private:
    Impl(const std::string& file_backend_filename, const Filter& filter_)
        : filter{filter_}, file_backend{file_backend_filename}, backend_thread{std::thread([this] {
              Common::SetCurrentThreadName("citra:Log");
              Entry entry;
              const auto write_logs = [this, &entry]() {
                  ForEachBackend([&entry](Backend& backend) { backend.Write(entry); });
              };
              while (true) {
                  entry = message_queue.PopWait();
                  if (entry.final_entry) {
                      break;
                  }
                  write_logs();
              }
              // Drain the logging queue. Only writes out up to MAX_LOGS_TO_WRITE to prevent a
              // case where a system is repeatedly spamming logs even on close.
              int max_logs_to_write = filter.IsDebug() ? INT_MAX : 100;
              while (max_logs_to_write-- && message_queue.Pop(entry)) {
                  write_logs();
              }
          })} {}

    ~Impl() {
        StopBackendThread();
    }

    void StopBackendThread() {
        Entry stop_entry{};
        stop_entry.final_entry = true;
        message_queue.Push(stop_entry);
        backend_thread.join();
    }

    Entry CreateEntry(Class log_class, Level log_level, const char* filename, unsigned int line_nr,
                      const char* function, std::string message) const {
        using std::chrono::duration_cast;
        using std::chrono::microseconds;
        using std::chrono::steady_clock;

        return {
            .timestamp = duration_cast<microseconds>(steady_clock::now() - time_origin),
            .log_class = log_class,
            .log_level = log_level,
            .filename = filename,
            .line_num = line_nr,
            .function = function,
            .message = std::move(message),
            .final_entry = false,
        };
    }

    void ForEachBackend(auto lambda) {
        lambda(static_cast<Backend&>(debugger_backend));
        lambda(static_cast<Backend&>(color_console_backend));
        lambda(static_cast<Backend&>(file_backend));
    }

    static void Deleter(Impl* ptr) {
        delete ptr;
    }

    static inline std::unique_ptr<Impl, decltype(&Deleter)> instance{nullptr, Deleter};

    Filter filter;
    DebuggerBackend debugger_backend{};
    ColorConsoleBackend color_console_backend{};
    FileBackend file_backend;

    std::thread backend_thread;
    MPSCQueue<Entry> message_queue{};
    std::chrono::steady_clock::time_point time_origin{std::chrono::steady_clock::now()};
};
} // namespace

void Initialize() {
    Impl::Initialize();
}

void DisableLoggingInTests() {
    initialization_in_progress_suppress_logging = true;
}

void SetGlobalFilter(const Filter& filter) {
    Impl::Instance().SetGlobalFilter(filter);
}

void SetColorConsoleBackendEnabled(bool enabled) {
    Impl::Instance().SetColorConsoleBackendEnabled(enabled);
}

void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename,
                       unsigned int line_num, const char* function, const char* format,
                       const fmt::format_args& args) {
    if (!initialization_in_progress_suppress_logging) {
        Impl::Instance().PushEntry(log_class, log_level, filename, line_num, function,
                                   fmt::vformat(format, args));
    }
}
} // namespace Common::Log
