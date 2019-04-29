// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <share.h>   // For _SH_DENYWR
#include <windows.h> // For OutputDebugStringW
#else
#define _SH_DENYWR 0
#endif
#include "common/assert.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/logging/text_formatter.h"
#include "common/string_util.h"
#include "common/threadsafe_queue.h"

namespace Log {

/**
 * Static state as a singleton.
 */
class Impl {
public:
    static Impl& Instance() {
        static Impl backend;
        return backend;
    }

    Impl(Impl const&) = delete;
    const Impl& operator=(Impl const&) = delete;

    void PushEntry(Class log_class, Level log_level, const char* filename, unsigned int line_num,
                   const char* function, std::string message) {
        message_queue.Push(
            CreateEntry(log_class, log_level, filename, line_num, function, std::move(message)));
    }

    void AddBackend(std::unique_ptr<Backend> backend) {
        std::lock_guard lock{writing_mutex};
        backends.push_back(std::move(backend));
    }

    void RemoveBackend(std::string_view backend_name) {
        std::lock_guard lock{writing_mutex};
        const auto it =
            std::remove_if(backends.begin(), backends.end(),
                           [&backend_name](const auto& i) { return backend_name == i->GetName(); });
        backends.erase(it, backends.end());
    }

    const Filter& GetGlobalFilter() const {
        return filter;
    }

    void SetGlobalFilter(const Filter& f) {
        filter = f;
    }

    Backend* GetBackend(std::string_view backend_name) {
        const auto it =
            std::find_if(backends.begin(), backends.end(),
                         [&backend_name](const auto& i) { return backend_name == i->GetName(); });
        if (it == backends.end())
            return nullptr;
        return it->get();
    }

private:
    Impl() {
        backend_thread = std::thread([&] {
            Entry entry;
            auto write_logs = [&](Entry& e) {
                std::lock_guard lock{writing_mutex};
                for (const auto& backend : backends) {
                    backend->Write(e);
                }
            };
            while (true) {
                entry = message_queue.PopWait();
                if (entry.final_entry) {
                    break;
                }
                write_logs(entry);
            }

            // Drain the logging queue. Only writes out up to MAX_LOGS_TO_WRITE to prevent a case
            // where a system is repeatedly spamming logs even on close.
            constexpr int MAX_LOGS_TO_WRITE = 100;
            int logs_written = 0;
            while (logs_written++ < MAX_LOGS_TO_WRITE && message_queue.Pop(entry)) {
                write_logs(entry);
            }
        });
    }

    ~Impl() {
        Entry entry;
        entry.final_entry = true;
        message_queue.Push(entry);
        backend_thread.join();
    }

    Entry CreateEntry(Class log_class, Level log_level, const char* filename, unsigned int line_nr,
                      const char* function, std::string message) const {
        using std::chrono::duration_cast;
        using std::chrono::steady_clock;

        // matches from the beginning up to the last '../' or 'src/'
        static const std::regex trim_source_path(R"(.*([\/\\]|^)((\.\.)|(src))[\/\\])");

        Entry entry;
        entry.timestamp =
            duration_cast<std::chrono::microseconds>(steady_clock::now() - time_origin);
        entry.log_class = log_class;
        entry.log_level = log_level;
        entry.filename = std::regex_replace(filename, trim_source_path, "");
        entry.line_num = line_nr;
        entry.function = function;
        entry.message = std::move(message);

        return entry;
    }

    std::mutex writing_mutex;
    std::thread backend_thread;
    std::vector<std::unique_ptr<Backend>> backends;
    Common::MPSCQueue<Log::Entry> message_queue;
    Filter filter;
    std::chrono::steady_clock::time_point time_origin{std::chrono::steady_clock::now()};
};

void ConsoleBackend::Write(const Entry& entry) {
    PrintMessage(entry);
}

void ColorConsoleBackend::Write(const Entry& entry) {
    PrintColoredMessage(entry);
}

// _SH_DENYWR allows read only access to the file for other programs.
// It is #defined to 0 on other platforms
FileBackend::FileBackend(const std::string& filename)
    : file(filename, "w", _SH_DENYWR), bytes_written(0) {}

void FileBackend::Write(const Entry& entry) {
    // prevent logs from going over the maximum size (in case its spamming and the user doesn't
    // know)
    constexpr std::size_t MAX_BYTES_WRITTEN = 50 * 1024L * 1024L;
    if (!file.IsOpen() || bytes_written > MAX_BYTES_WRITTEN) {
        return;
    }
    bytes_written += file.WriteString(FormatLogMessage(entry).append(1, '\n'));
    if (entry.log_level >= Level::Error) {
        file.Flush();
    }
}

void DebuggerBackend::Write(const Entry& entry) {
#ifdef _WIN32
    ::OutputDebugStringW(Common::UTF8ToUTF16W(FormatLogMessage(entry).append(1, '\n')).c_str());
#endif
}

/// Macro listing all log classes. Code should define CLS and SUB as desired before invoking this.
#define ALL_LOG_CLASSES()                                                                          \
    CLS(Log)                                                                                       \
    CLS(Common)                                                                                    \
    SUB(Common, Filesystem)                                                                        \
    SUB(Common, Memory)                                                                            \
    CLS(Core)                                                                                      \
    SUB(Core, ARM11)                                                                               \
    SUB(Core, Timing)                                                                              \
    SUB(Core, Cheats)                                                                              \
    CLS(Config)                                                                                    \
    CLS(Debug)                                                                                     \
    SUB(Debug, Emulated)                                                                           \
    SUB(Debug, GPU)                                                                                \
    SUB(Debug, Breakpoint)                                                                         \
    SUB(Debug, GDBStub)                                                                            \
    CLS(Kernel)                                                                                    \
    SUB(Kernel, SVC)                                                                               \
    CLS(Applet)                                                                                    \
    SUB(Applet, SWKBD)                                                                             \
    CLS(Service)                                                                                   \
    SUB(Service, SRV)                                                                              \
    SUB(Service, FRD)                                                                              \
    SUB(Service, FS)                                                                               \
    SUB(Service, ERR)                                                                              \
    SUB(Service, APT)                                                                              \
    SUB(Service, BOSS)                                                                             \
    SUB(Service, GSP)                                                                              \
    SUB(Service, AC)                                                                               \
    SUB(Service, AM)                                                                               \
    SUB(Service, PTM)                                                                              \
    SUB(Service, LDR)                                                                              \
    SUB(Service, MIC)                                                                              \
    SUB(Service, NDM)                                                                              \
    SUB(Service, NFC)                                                                              \
    SUB(Service, NIM)                                                                              \
    SUB(Service, NS)                                                                               \
    SUB(Service, NWM)                                                                              \
    SUB(Service, CAM)                                                                              \
    SUB(Service, CECD)                                                                             \
    SUB(Service, CFG)                                                                              \
    SUB(Service, CSND)                                                                             \
    SUB(Service, DSP)                                                                              \
    SUB(Service, DLP)                                                                              \
    SUB(Service, HID)                                                                              \
    SUB(Service, HTTP)                                                                             \
    SUB(Service, SOC)                                                                              \
    SUB(Service, IR)                                                                               \
    SUB(Service, Y2R)                                                                              \
    SUB(Service, PS)                                                                               \
    CLS(HW)                                                                                        \
    SUB(HW, Memory)                                                                                \
    SUB(HW, LCD)                                                                                   \
    SUB(HW, GPU)                                                                                   \
    SUB(HW, AES)                                                                                   \
    CLS(Frontend)                                                                                  \
    CLS(Render)                                                                                    \
    SUB(Render, Software)                                                                          \
    SUB(Render, OpenGL)                                                                            \
    CLS(Audio)                                                                                     \
    SUB(Audio, DSP)                                                                                \
    SUB(Audio, Sink)                                                                               \
    CLS(Input)                                                                                     \
    CLS(Network)                                                                                   \
    CLS(Movie)                                                                                     \
    CLS(Loader)                                                                                    \
    CLS(WebService)                                                                                \
    CLS(RPC_Server)

// GetClassName is a macro defined by Windows.h, grrr...
const char* GetLogClassName(Class log_class) {
    switch (log_class) {
#define CLS(x)                                                                                     \
    case Class::x:                                                                                 \
        return #x;
#define SUB(x, y)                                                                                  \
    case Class::x##_##y:                                                                           \
        return #x "." #y;
        ALL_LOG_CLASSES()
#undef CLS
#undef SUB
    case Class::Count:
        UNREACHABLE();
    }
}

const char* GetLevelName(Level log_level) {
#define LVL(x)                                                                                     \
    case Level::x:                                                                                 \
        return #x
    switch (log_level) {
        LVL(Trace);
        LVL(Debug);
        LVL(Info);
        LVL(Warning);
        LVL(Error);
        LVL(Critical);
    case Level::Count:
        UNREACHABLE();
    }
#undef LVL
}

void SetGlobalFilter(const Filter& filter) {
    Impl::Instance().SetGlobalFilter(filter);
}

void AddBackend(std::unique_ptr<Backend> backend) {
    Impl::Instance().AddBackend(std::move(backend));
}

void RemoveBackend(std::string_view backend_name) {
    Impl::Instance().RemoveBackend(backend_name);
}

Backend* GetBackend(std::string_view backend_name) {
    return Impl::Instance().GetBackend(backend_name);
}

void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename,
                       unsigned int line_num, const char* function, const char* format,
                       const fmt::format_args& args) {
    auto& instance = Impl::Instance();
    const auto& filter = instance.GetGlobalFilter();
    if (!filter.CheckMessage(log_class, log_level))
        return;

    instance.PushEntry(log_class, log_level, filename, line_num, function,
                       fmt::vformat(format, args));
}
} // namespace Log
