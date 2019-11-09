// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <thread>

// This needs to be included before getopt.h because the latter #defines symbols used by it
#include "common/microprofile.h"

#ifdef _WIN32
// windows.h needs to be included before shellapi.h
#include <windows.h>

#include <shellapi.h>
#endif

#include "citra/config.h"
#include "citra/emu_window/emu_window_sdl2.h"
#include "citra/lodepng_image_interface.h"
#include "common/common_paths.h"
#include "common/detached_tasks.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/dumping/backend.h"
#include "core/file_sys/cia_container.h"
#include "core/frontend/applets/default_applets.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/settings.h"
#include "network/network.h"
#include "video_core/video_core.h"

#undef _UNICODE
#include <getopt.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _WIN32
extern "C" {
// tells Nvidia drivers to use the dedicated GPU by default on laptops with switchable graphics
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}
#endif

static void PrintHelp(const char* argv0) {
    std::cout << "Usage: " << argv0
              << " [options] <filename>\n"
                 "-g, --gdbport=NUMBER Enable gdb stub on port NUMBER\n"
                 "-i, --install=FILE    Installs a specified CIA file\n"
                 "-m, --multiplayer=nick:password@address:port"
                 " Nickname, password, address and port for multiplayer\n"
                 "-r, --movie-record=[file]  Record a movie (game inputs) to the given file\n"
                 "-p, --movie-play=[file]    Playback the movie (game inputs) from the given file\n"
                 "-d, --dump-video=[file]    Dumps audio and video to the given video file\n"
                 "-f, --fullscreen     Start in fullscreen mode\n"
                 "-h, --help           Display this help and exit\n"
                 "-v, --version        Output version information and exit\n";
}

static void PrintVersion() {
    std::cout << "Citra " << Common::g_scm_branch << " " << Common::g_scm_desc << std::endl;
}

static void OnStateChanged(const Network::RoomMember::State& state) {
    switch (state) {
    case Network::RoomMember::State::Idle:
        LOG_DEBUG(Network, "Network is idle");
        break;
    case Network::RoomMember::State::Joining:
        LOG_DEBUG(Network, "Connection sequence to room started");
        break;
    case Network::RoomMember::State::Joined:
        LOG_DEBUG(Network, "Successfully joined to the room");
        break;
    case Network::RoomMember::State::Moderator:
        LOG_DEBUG(Network, "Successfully joined the room as a moderator");
        break;
    default:
        break;
    }
}

static void OnNetworkError(const Network::RoomMember::Error& error) {
    switch (error) {
    case Network::RoomMember::Error::LostConnection:
        LOG_DEBUG(Network, "Lost connection to the room");
        break;
    case Network::RoomMember::Error::CouldNotConnect:
        LOG_ERROR(Network, "Error: Could not connect");
        exit(1);
        break;
    case Network::RoomMember::Error::NameCollision:
        LOG_ERROR(
            Network,
            "You tried to use the same nickname as another user that is connected to the Room");
        exit(1);
        break;
    case Network::RoomMember::Error::MacCollision:
        LOG_ERROR(Network, "You tried to use the same MAC-Address as another user that is "
                           "connected to the Room");
        exit(1);
        break;
    case Network::RoomMember::Error::ConsoleIdCollision:
        LOG_ERROR(Network, "Your Console ID conflicted with someone else in the Room");
        exit(1);
        break;
    case Network::RoomMember::Error::WrongPassword:
        LOG_ERROR(Network, "Room replied with: Wrong password");
        exit(1);
        break;
    case Network::RoomMember::Error::WrongVersion:
        LOG_ERROR(Network,
                  "You are using a different version than the room you are trying to connect to");
        exit(1);
        break;
    case Network::RoomMember::Error::RoomIsFull:
        LOG_ERROR(Network, "The room is full");
        exit(1);
        break;
    case Network::RoomMember::Error::HostKicked:
        LOG_ERROR(Network, "You have been kicked by the host");
        break;
    case Network::RoomMember::Error::HostBanned:
        LOG_ERROR(Network, "You have been banned by the host");
        break;
    }
}

static void OnMessageReceived(const Network::ChatEntry& msg) {
    std::cout << std::endl << msg.nickname << ": " << msg.message << std::endl << std::endl;
}

static void OnStatusMessageReceived(const Network::StatusMessageEntry& msg) {
    std::string message;
    switch (msg.type) {
    case Network::IdMemberJoin:
        message = fmt::format("{} has joined", msg.nickname);
        break;
    case Network::IdMemberLeave:
        message = fmt::format("{} has left", msg.nickname);
        break;
    case Network::IdMemberKicked:
        message = fmt::format("{} has been kicked", msg.nickname);
        break;
    case Network::IdMemberBanned:
        message = fmt::format("{} has been banned", msg.nickname);
        break;
    case Network::IdAddressUnbanned:
        message = fmt::format("{} has been unbanned", msg.nickname);
        break;
    }
    if (!message.empty())
        std::cout << std::endl << "* " << message << std::endl << std::endl;
}

static void InitializeLogging() {
    Log::Filter log_filter(Log::Level::Debug);
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);

    Log::AddBackend(std::make_unique<Log::ColorConsoleBackend>());

    const std::string& log_dir = FileUtil::GetUserPath(FileUtil::UserPath::LogDir);
    FileUtil::CreateFullPath(log_dir);
    Log::AddBackend(std::make_unique<Log::FileBackend>(log_dir + LOG_FILE));
#ifdef _WIN32
    Log::AddBackend(std::make_unique<Log::DebuggerBackend>());
#endif
}

/// Application entry point
int main(int argc, char** argv) {
    Common::DetachedTasks detached_tasks;
    Config config;
    int option_index = 0;
    bool use_gdbstub = Settings::values.use_gdbstub;
    u32 gdb_port = static_cast<u32>(Settings::values.gdbstub_port);
    std::string movie_record;
    std::string movie_play;
    std::string dump_video;

    InitializeLogging();

    char* endarg;
#ifdef _WIN32
    int argc_w;
    auto argv_w = CommandLineToArgvW(GetCommandLineW(), &argc_w);

    if (argv_w == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to get command line arguments");
        return -1;
    }
#endif
    std::string filepath;

    bool use_multiplayer = false;
    bool fullscreen = false;
    std::string nickname{};
    std::string password{};
    std::string address{};
    u16 port = Network::DefaultRoomPort;

    static struct option long_options[] = {
        {"gdbport", required_argument, 0, 'g'},     {"install", required_argument, 0, 'i'},
        {"multiplayer", required_argument, 0, 'm'}, {"movie-record", required_argument, 0, 'r'},
        {"movie-play", required_argument, 0, 'p'},  {"dump-video", required_argument, 0, 'd'},
        {"fullscreen", no_argument, 0, 'f'},        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},           {0, 0, 0, 0},
    };

    while (optind < argc) {
        int arg = getopt_long(argc, argv, "g:i:m:r:p:fhv", long_options, &option_index);
        if (arg != -1) {
            switch (static_cast<char>(arg)) {
            case 'g':
                errno = 0;
                gdb_port = strtoul(optarg, &endarg, 0);
                use_gdbstub = true;
                if (endarg == optarg)
                    errno = EINVAL;
                if (errno != 0) {
                    perror("--gdbport");
                    exit(1);
                }
                break;
            case 'i': {
                const auto cia_progress = [](std::size_t written, std::size_t total) {
                    LOG_INFO(Frontend, "{:02d}%", (written * 100 / total));
                };
                if (Service::AM::InstallCIA(std::string(optarg), cia_progress) !=
                    Service::AM::InstallStatus::Success)
                    errno = EINVAL;
                if (errno != 0)
                    exit(1);
                break;
            }
            case 'm': {
                use_multiplayer = true;
                const std::string str_arg(optarg);
                // regex to check if the format is nickname:password@ip:port
                // with optional :password
                const std::regex re("^([^:]+)(?::(.+))?@([^:]+)(?::([0-9]+))?$");
                if (!std::regex_match(str_arg, re)) {
                    std::cout << "Wrong format for option --multiplayer\n";
                    PrintHelp(argv[0]);
                    return 0;
                }

                std::smatch match;
                std::regex_search(str_arg, match, re);
                ASSERT(match.size() == 5);
                nickname = match[1];
                password = match[2];
                address = match[3];
                if (!match[4].str().empty())
                    port = std::stoi(match[4]);
                std::regex nickname_re("^[a-zA-Z0-9._\\- ]+$");
                if (!std::regex_match(nickname, nickname_re)) {
                    std::cout
                        << "Nickname is not valid. Must be 4 to 20 alphanumeric characters.\n";
                    return 0;
                }
                if (address.empty()) {
                    std::cout << "Address to room must not be empty.\n";
                    return 0;
                }
                break;
            }
            case 'r':
                movie_record = optarg;
                break;
            case 'p':
                movie_play = optarg;
                break;
            case 'd':
                dump_video = optarg;
                break;
            case 'f':
                fullscreen = true;
                LOG_INFO(Frontend, "Starting in fullscreen mode...");
                break;
            case 'h':
                PrintHelp(argv[0]);
                return 0;
            case 'v':
                PrintVersion();
                return 0;
            }
        } else {
#ifdef _WIN32
            filepath = Common::UTF16ToUTF8(argv_w[optind]);
#else
            filepath = argv[optind];
#endif
            optind++;
        }
    }

#ifdef _WIN32
    LocalFree(argv_w);
#endif

    MicroProfileOnThreadCreate("EmuThread");
    SCOPE_EXIT({ MicroProfileShutdown(); });

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
        return -1;
    }

    if (!movie_record.empty() && !movie_play.empty()) {
        LOG_CRITICAL(Frontend, "Cannot both play and record a movie");
        return -1;
    }

    if (!movie_record.empty()) {
        Core::Movie::GetInstance().PrepareForRecording();
    }
    if (!movie_play.empty()) {
        Core::Movie::GetInstance().PrepareForPlayback(movie_play);
    }

    // Apply the command line arguments
    Settings::values.gdbstub_port = gdb_port;
    Settings::values.use_gdbstub = use_gdbstub;
    Settings::Apply();

    // Register frontend applets
    Frontend::RegisterDefaultApplets();

    // Register generic image interface
    Core::System::GetInstance().RegisterImageInterface(std::make_shared<LodePNGImageInterface>());

    std::unique_ptr<EmuWindow_SDL2> emu_window{std::make_unique<EmuWindow_SDL2>(fullscreen)};

    Core::System& system{Core::System::GetInstance()};

    const Core::System::ResultStatus load_result{system.Load(*emu_window, filepath)};

    switch (load_result) {
    case Core::System::ResultStatus::ErrorGetLoader:
        LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", filepath);
        return -1;
    case Core::System::ResultStatus::ErrorLoader:
        LOG_CRITICAL(Frontend, "Failed to load ROM!");
        return -1;
    case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted:
        LOG_CRITICAL(Frontend, "The game that you are trying to load must be decrypted before "
                               "being used with Citra. \n\n For more information on dumping and "
                               "decrypting games, please refer to: "
                               "https://citra-emu.org/wiki/dumping-game-cartridges/");
        return -1;
    case Core::System::ResultStatus::ErrorLoader_ErrorInvalidFormat:
        LOG_CRITICAL(Frontend, "Error while loading ROM: The ROM format is not supported.");
        return -1;
    case Core::System::ResultStatus::ErrorNotInitialized:
        LOG_CRITICAL(Frontend, "CPUCore not initialized");
        return -1;
    case Core::System::ResultStatus::ErrorSystemMode:
        LOG_CRITICAL(Frontend, "Failed to determine system mode!");
        return -1;
    case Core::System::ResultStatus::ErrorVideoCore:
        LOG_CRITICAL(Frontend, "VideoCore not initialized");
        return -1;
    case Core::System::ResultStatus::Success:
        break; // Expected case
    }

    system.TelemetrySession().AddField(Telemetry::FieldType::App, "Frontend", "SDL");

    if (use_multiplayer) {
        if (auto member = Network::GetRoomMember().lock()) {
            member->BindOnChatMessageRecieved(OnMessageReceived);
            member->BindOnStatusMessageReceived(OnStatusMessageReceived);
            member->BindOnStateChanged(OnStateChanged);
            member->BindOnError(OnNetworkError);
            LOG_DEBUG(Network, "Start connection to {}:{} with nickname {}", address, port,
                      nickname);
            member->Join(nickname, Service::CFG::GetConsoleIdHash(system), address.c_str(), port, 0,
                         Network::NoPreferredMac, password);
        } else {
            LOG_ERROR(Network, "Could not access RoomMember");
            return 0;
        }
    }

    if (!movie_play.empty()) {
        Core::Movie::GetInstance().StartPlayback(movie_play);
    }
    if (!movie_record.empty()) {
        Core::Movie::GetInstance().StartRecording(movie_record);
    }
    if (!dump_video.empty()) {
        Layout::FramebufferLayout layout{
            Layout::FrameLayoutFromResolutionScale(VideoCore::GetResolutionScaleFactor())};
        system.VideoDumper().StartDumping(dump_video, "webm", layout);
    }

    while (emu_window->IsOpen()) {
        system.RunLoop();
    }

    Core::Movie::GetInstance().Shutdown();
    if (system.VideoDumper().IsDumping()) {
        system.VideoDumper().StopDumping();
    }

    system.Shutdown();

    detached_tasks.WaitForAllTasks();
    return 0;
}
