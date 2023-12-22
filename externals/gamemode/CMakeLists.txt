# SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

project(gamemode LANGUAGES CXX C)

add_library(gamemode include/gamemode_client.h)

target_include_directories(gamemode PUBLIC include)
set_target_properties(gamemode PROPERTIES LINKER_LANGUAGE C)
