// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <glad/glad.h>

#ifdef _MSC_VER
#include <getopt.h>
#else
#include <getopt.h>
#include <unistd.h>
#endif

#ifdef _WIN32
// windows.h needs to be included before shellapi.h
#include <windows.h>

#include <shellapi.h>
#endif

#include "common/common_types.h"
#include "common/scm_rev.h"
#include "core/announce_multiplayer_session.h"
#include "core/core.h"
#include "core/settings.h"
#include "network/network.h"

static void PrintHelp(const char* argv0) {
    std::cout << "Usage: " << argv0
              << " [options] <filename>\n"
                 "--room-name         The name of the room\n"
                 "--port              The port used for the room\n"
                 "--max_members       The maximum number of players for this room\n"
                 "--password          The password for the room\n"
                 "--preferred-game    The preferred game for this room\n"
                 "--preferred-game-id The preferred game-id for this room\n"
                 "--username          The username used for announce\n"
                 "--token             The token used for announce\n"
                 "--web-api-url       Citra Web API url\n"
                 "-h, --help          Display this help and exit\n"
                 "-v, --version       Output version information and exit\n";
}

static void PrintVersion() {
    std::cout << "Citra dedicated room " << Common::g_scm_branch << " " << Common::g_scm_desc
              << " Libnetwork: " << Network::network_version << std::endl;
}

/// Application entry point
int main(int argc, char** argv) {
    int option_index = 0;
    char* endarg;

    // This is just to be able to link against core
    gladLoadGL();

    std::string room_name;
    std::string password;
    std::string preferred_game;
    std::string username;
    std::string token;
    std::string web_api_url;
    u64 preferred_game_id = 0;
    u32 port = Network::DefaultRoomPort;
    u32 max_members = 16;

    static struct option long_options[] = {
        {"room-name", required_argument, 0, 'n'},
        {"port", required_argument, 0, 'p'},
        {"max_members", required_argument, 0, 'm'},
        {"password", required_argument, 0, 'w'},
        {"preferred-game", required_argument, 0, 'g'},
        {"preferred-game-id", required_argument, 0, 'i'},
        {"username", required_argument, 0, 'u'},
        {"token", required_argument, 0, 't'},
        {"web-api-url", required_argument, 0, 'a'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0},
    };

    while (optind < argc) {
        char arg = getopt_long(argc, argv, "n:p:m:w:g:u:t:a:i:hv", long_options, &option_index);
        if (arg != -1) {
            switch (arg) {
            case 'n':
                room_name.assign(optarg);
                break;
            case 'p':
                port = strtoul(optarg, &endarg, 0);
                break;
            case 'm':
                max_members = strtoul(optarg, &endarg, 0);
                break;
            case 'w':
                password.assign(optarg);
                break;
            case 'g':
                preferred_game.assign(optarg);
                break;
            case 'i':
                preferred_game_id = strtoull(optarg, &endarg, 16);
                break;
            case 'u':
                username.assign(optarg);
                break;
            case 't':
                token.assign(optarg);
                break;
            case 'a':
                web_api_url.assign(optarg);
                break;
            case 'h':
                PrintHelp(argv[0]);
                return 0;
            case 'v':
                PrintVersion();
                return 0;
            }
        }
    }

    if (room_name.empty()) {
        std::cout << "room name is empty!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (preferred_game.empty()) {
        std::cout << "preferred game is empty!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (preferred_game_id == 0) {
        std::cout << "preferred-game-id not set!\nThis should get set to allow users to find your "
                     "room.\nSet with --preferred-game-id id\n\n";
    }
    if (max_members >= Network::MaxConcurrentConnections || max_members < 2) {
        std::cout << "max_members needs to be in the range 2 - "
                  << Network::MaxConcurrentConnections << "!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (port > 65535) {
        std::cout << "port needs to be in the range 0 - 65535!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    bool announce = true;
    if (username.empty()) {
        announce = false;
        std::cout << "username is empty: Hosting a private room\n\n";
    }
    if (token.empty() && announce) {
        announce = false;
        std::cout << "token is empty: Hosting a private room\n\n";
    }
    if (web_api_url.empty() && announce) {
        announce = false;
        std::cout << "endpoint url is empty: Hosting a private room\n\n";
    }
    if (announce) {
        std::cout << "Hosting a public room\n\n";
        Settings::values.web_api_url = web_api_url;
        Settings::values.citra_username = username;
        Settings::values.citra_token = token;
    }

    Network::Init();
    if (std::shared_ptr<Network::Room> room = Network::GetRoom().lock()) {
        if (!room->Create(room_name, "", port, password, max_members, preferred_game,
                          preferred_game_id)) {
            std::cout << "Failed to create room: \n\n";
            return -1;
        }
        std::cout << "Room is open. Close with Q+Enter...\n\n";
        auto announce_session = std::make_unique<Core::AnnounceMultiplayerSession>();
        if (announce) {
            announce_session->Start();
        }
        while (room->GetState() == Network::Room::State::Open) {
            std::string in;
            std::cin >> in;
            if (in.size() > 0) {
                if (announce) {
                    announce_session->Stop();
                }
                announce_session.reset();
                room->Destroy();
                Network::Shutdown();
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (announce) {
            announce_session->Stop();
        }
        announce_session.reset();
        room->Destroy();
    }
    Network::Shutdown();
    return 0;
}
