// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Common::Linux {

/**
 * Start the (Feral Interactive) Linux gamemode if it is installed and it is activated
 */
void StartGamemode();

/**
 * Stop the (Feral Interactive) Linux gamemode if it is installed and it is activated
 */
void StopGamemode();

/**
 * Start or stop the (Feral Interactive) Linux gamemode if it is installed and it is activated
 * @param state The new state the gamemode should have
 */
void SetGamemodeState(bool state);

} // namespace Common::Linux
