// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#include "common/log_manager.h"
#include "common/console_listener.h"
#include "common/timer.h"

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file, int line,
    const char* function, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (LogManager::GetInstance()) {
        LogManager::GetInstance()->Log(level, type,
            file, line, function, fmt, args);
    }
    va_end(args);
}

LogManager *LogManager::m_logManager = nullptr;

LogManager::LogManager()
{
    // create log files
    m_Log[LogTypes::MASTER_LOG]         = new LogContainer("*",                 "Master Log");
    m_Log[LogTypes::BOOT]               = new LogContainer("BOOT",              "Boot");
    m_Log[LogTypes::COMMON]             = new LogContainer("COMMON",            "Common");
    m_Log[LogTypes::CONFIG]             = new LogContainer("CONFIG",            "Configuration");
    m_Log[LogTypes::DISCIO]             = new LogContainer("DIO",               "Disc IO");
    m_Log[LogTypes::FILEMON]            = new LogContainer("FileMon",           "File Monitor");
    m_Log[LogTypes::PAD]                = new LogContainer("PAD",               "Pad");
    m_Log[LogTypes::PIXELENGINE]        = new LogContainer("PE",                "PixelEngine");
    m_Log[LogTypes::COMMANDPROCESSOR]   = new LogContainer("CP",                "CommandProc");
    m_Log[LogTypes::VIDEOINTERFACE]     = new LogContainer("VI",                "VideoInt");
    m_Log[LogTypes::SERIALINTERFACE]    = new LogContainer("SI",                "SerialInt");
    m_Log[LogTypes::PROCESSORINTERFACE] = new LogContainer("PI",                "ProcessorInt");
    m_Log[LogTypes::MEMMAP]             = new LogContainer("MI",                "MI & memmap");
    m_Log[LogTypes::SP1]                = new LogContainer("SP1",               "Serial Port 1");
    m_Log[LogTypes::STREAMINGINTERFACE] = new LogContainer("Stream",            "StreamingInt");
    m_Log[LogTypes::DSPINTERFACE]       = new LogContainer("DSP",               "DSPInterface");
    m_Log[LogTypes::DVDINTERFACE]       = new LogContainer("DVD",               "DVDInterface");
    m_Log[LogTypes::GSP]                = new LogContainer("GSP",               "GSP");
    m_Log[LogTypes::EXPANSIONINTERFACE] = new LogContainer("EXI",               "ExpansionInt");
    m_Log[LogTypes::GDB_STUB]           = new LogContainer("GDB_STUB",          "GDB Stub");
    m_Log[LogTypes::AUDIO_INTERFACE]    = new LogContainer("AI",                "AudioInt");
    m_Log[LogTypes::ARM11]              = new LogContainer("ARM11",             "ARM11");
    m_Log[LogTypes::OSHLE]              = new LogContainer("HLE",               "HLE");
    m_Log[LogTypes::DSPHLE]             = new LogContainer("DSPHLE",            "DSP HLE");
    m_Log[LogTypes::DSPLLE]             = new LogContainer("DSPLLE",            "DSP LLE");
    m_Log[LogTypes::DSP_MAIL]           = new LogContainer("DSPMails",          "DSP Mails");
    m_Log[LogTypes::VIDEO]              = new LogContainer("Video",             "Video Backend");
    m_Log[LogTypes::AUDIO]              = new LogContainer("Audio",             "Audio Emulator");
    m_Log[LogTypes::DYNA_REC]           = new LogContainer("JIT",               "JIT");
    m_Log[LogTypes::CONSOLE]            = new LogContainer("CONSOLE",           "Dolphin Console");
    m_Log[LogTypes::OSREPORT]           = new LogContainer("OSREPORT",          "OSReport");
    m_Log[LogTypes::TIME]               = new LogContainer("Time",              "Core Timing");
    m_Log[LogTypes::LOADER]             = new LogContainer("Loader",            "Loader");
    m_Log[LogTypes::FILESYS]            = new LogContainer("FileSys",           "File System");
    m_Log[LogTypes::WII_IPC_HID]        = new LogContainer("WII_IPC_HID",       "WII IPC HID");
    m_Log[LogTypes::KERNEL]             = new LogContainer("KERNEL",            "KERNEL HLE");
    m_Log[LogTypes::WII_IPC_DVD]        = new LogContainer("WII_IPC_DVD",       "WII IPC DVD");
    m_Log[LogTypes::WII_IPC_ES]         = new LogContainer("WII_IPC_ES",        "WII IPC ES");
    m_Log[LogTypes::WII_IPC_FILEIO]     = new LogContainer("WII_IPC_FILEIO",    "WII IPC FILEIO");
    m_Log[LogTypes::RENDER]             = new LogContainer("RENDER",            "RENDER");
    m_Log[LogTypes::GPU]                = new LogContainer("GPU",               "GPU");
    m_Log[LogTypes::SVC]                = new LogContainer("SVC",               "Supervisor Call HLE");
    m_Log[LogTypes::NDMA]               = new LogContainer("NDMA",              "NDMA");
    m_Log[LogTypes::HLE]                = new LogContainer("HLE",               "High Level Emulation");
    m_Log[LogTypes::HW]                 = new LogContainer("HW",                "Hardware");
    m_Log[LogTypes::ACTIONREPLAY]       = new LogContainer("ActionReplay",      "ActionReplay");
    m_Log[LogTypes::MEMCARD_MANAGER]    = new LogContainer("MemCard Manager",   "MemCard Manager");
    m_Log[LogTypes::NETPLAY]            = new LogContainer("NETPLAY",           "Netplay");
    m_Log[LogTypes::GUI]                = new LogContainer("GUI",               "GUI");

    m_fileLog = new FileLogListener(FileUtil::GetUserPath(F_MAINLOG_IDX).c_str());
    m_consoleLog = new ConsoleListener();
    m_debuggerLog = new DebuggerLogListener();

    for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
    {
        m_Log[i]->SetEnable(true);
        m_Log[i]->AddListener(m_fileLog);
        m_Log[i]->AddListener(m_consoleLog);
#ifdef _MSC_VER
        if (IsDebuggerPresent())
            m_Log[i]->AddListener(m_debuggerLog);
#endif
    }

    m_consoleLog->Open();
}

LogManager::~LogManager()
{
    for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
    {
        m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_fileLog);
        m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_consoleLog);
        m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_debuggerLog);
    }

    for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
        delete m_Log[i];

    delete m_fileLog;
    delete m_consoleLog;
    delete m_debuggerLog;
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file,
    int line, const char* function, const char *fmt, va_list args)
{
    char temp[MAX_MSGLEN];
    char msg[MAX_MSGLEN * 2];
    LogContainer *log = m_Log[type];

    if (!log->IsEnabled() || level > log->GetLevel() || ! log->HasListeners())
        return;

    Common::CharArrayFromFormatV(temp, MAX_MSGLEN, fmt, args);

    static const char level_to_char[7] = "ONEWID";
    sprintf(msg, "%s %s:%u %c[%s] %s: %s\n", Common::Timer::GetTimeFormatted().c_str(), file, line,
        level_to_char[(int)level], log->GetShortName(), function, temp);

#ifdef ANDROID
    Host_SysMessage(msg);
#endif
    log->Trigger(level, msg);
}

void LogManager::Init()
{
    m_logManager = new LogManager();
}

void LogManager::Shutdown()
{
    delete m_logManager;
    m_logManager = nullptr;
}

LogContainer::LogContainer(const char* shortName, const char* fullName, bool enable)
    : m_enable(enable)
{
    strncpy(m_fullName, fullName, 128);
    strncpy(m_shortName, shortName, 32);
    m_level = LogTypes::MAX_LOGLEVEL;
}

// LogContainer
void LogContainer::AddListener(LogListener *listener)
{
    std::lock_guard<std::mutex> lk(m_listeners_lock);
    m_listeners.insert(listener);
}

void LogContainer::RemoveListener(LogListener *listener)
{
    std::lock_guard<std::mutex> lk(m_listeners_lock);
    m_listeners.erase(listener);
}

void LogContainer::Trigger(LogTypes::LOG_LEVELS level, const char *msg)
{
    std::lock_guard<std::mutex> lk(m_listeners_lock);

    std::set<LogListener*>::const_iterator i;
    for (i = m_listeners.begin(); i != m_listeners.end(); ++i)
    {
        (*i)->Log(level, msg);
    }
}

FileLogListener::FileLogListener(const char *filename)
{
    OpenFStream(m_logfile, filename, std::ios::app);
    SetEnable(true);
}

void FileLogListener::Log(LogTypes::LOG_LEVELS, const char *msg)
{
    if (!IsEnabled() || !IsValid())
        return;

    std::lock_guard<std::mutex> lk(m_log_lock);
    m_logfile << msg << std::flush;
}

void DebuggerLogListener::Log(LogTypes::LOG_LEVELS, const char *msg)
{
#if _MSC_VER
    ::OutputDebugStringA(msg);
#endif
}
