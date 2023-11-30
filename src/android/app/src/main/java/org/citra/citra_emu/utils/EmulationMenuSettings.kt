// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import androidx.drawerlayout.widget.DrawerLayout
import androidx.preference.PreferenceManager
import org.citra.citra_emu.CitraApplication

object EmulationMenuSettings {
    private val preferences =
        PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)

    // These must match what is defined in src/common/settings.h
    const val LayoutOption_Default = 0
    const val LayoutOption_SingleScreen = 1
    const val LayoutOption_LargeScreen = 2
    const val LayoutOption_SideScreen = 3
    const val LayoutOption_MobilePortrait = 5
    const val LayoutOption_MobileLandscape = 6

    var joystickRelCenter: Boolean
        get() = preferences.getBoolean("EmulationMenuSettings_JoystickRelCenter", true)
        set(value) {
            preferences.edit()
                .putBoolean("EmulationMenuSettings_JoystickRelCenter", value)
                .apply()
        }
    var dpadSlide: Boolean
        get() = preferences.getBoolean("EmulationMenuSettings_DpadSlideEnable", true)
        set(value) {
            preferences.edit()
                .putBoolean("EmulationMenuSettings_DpadSlideEnable", value)
                .apply()
        }
    var landscapeScreenLayout: Int
        get() = preferences.getInt(
            "EmulationMenuSettings_LandscapeScreenLayout",
            LayoutOption_MobileLandscape
        )
        set(value) {
            preferences.edit()
                .putInt("EmulationMenuSettings_LandscapeScreenLayout", value)
                .apply()
        }
    var showFps: Boolean
        get() = preferences.getBoolean("EmulationMenuSettings_ShowFps", false)
        set(value) {
            preferences.edit()
                .putBoolean("EmulationMenuSettings_ShowFps", value)
                .apply()
        }
    var swapScreens: Boolean
        get() = preferences.getBoolean("EmulationMenuSettings_SwapScreens", false)
        set(value) {
            preferences.edit()
                .putBoolean("EmulationMenuSettings_SwapScreens", value)
                .apply()
        }
    var showOverlay: Boolean
        get() = preferences.getBoolean("EmulationMenuSettings_ShowOverlay", true)
        set(value) {
            preferences.edit()
                .putBoolean("EmulationMenuSettings_ShowOverlay", value)
                .apply()
        }
    var drawerLockMode: Int
        get() = preferences.getInt(
            "EmulationMenuSettings_DrawerLockMode",
            DrawerLayout.LOCK_MODE_UNLOCKED
        )
        set(value) {
            preferences.edit()
                .putInt("EmulationMenuSettings_DrawerLockMode", value)
                .apply()
        }
}
