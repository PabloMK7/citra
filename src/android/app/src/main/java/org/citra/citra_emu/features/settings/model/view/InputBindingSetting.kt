// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.settings.model.view

import android.content.Context
import android.content.SharedPreferences
import android.view.InputDevice
import android.view.InputDevice.MotionRange
import android.view.KeyEvent
import android.widget.Toast
import androidx.preference.PreferenceManager
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.R
import org.citra.citra_emu.features.hotkeys.Hotkey
import org.citra.citra_emu.features.settings.model.AbstractSetting
import org.citra.citra_emu.features.settings.model.Settings

class InputBindingSetting(
    val abstractSetting: AbstractSetting,
    titleId: Int
) : SettingsItem(abstractSetting, titleId, 0) {
    private val context: Context get() = CitraApplication.appContext
    private val preferences: SharedPreferences
        get() = PreferenceManager.getDefaultSharedPreferences(context)

    var value: String
        get() = preferences.getString(abstractSetting.key, "")!!
        set(string) {
            preferences.edit()
                .putString(abstractSetting.key, string)
                .apply()
        }

    /**
     * Returns true if this key is for the 3DS Circle Pad
     */
    fun isCirclePad(): Boolean =
        when (abstractSetting.key) {
            Settings.KEY_CIRCLEPAD_AXIS_HORIZONTAL,
            Settings.KEY_CIRCLEPAD_AXIS_VERTICAL -> true

            else -> false
        }

    /**
     * Returns true if this key is for a horizontal axis for a 3DS analog stick or D-pad
     */
    fun isHorizontalOrientation(): Boolean =
        when (abstractSetting.key) {
            Settings.KEY_CIRCLEPAD_AXIS_HORIZONTAL,
            Settings.KEY_CSTICK_AXIS_HORIZONTAL,
            Settings.KEY_DPAD_AXIS_HORIZONTAL -> true

            else -> false
        }

    /**
     * Returns true if this key is for the 3DS C-Stick
     */
    fun isCStick(): Boolean =
        when (abstractSetting.key) {
            Settings.KEY_CSTICK_AXIS_HORIZONTAL,
            Settings.KEY_CSTICK_AXIS_VERTICAL -> true

            else -> false
        }

    /**
     * Returns true if this key is for the 3DS D-Pad
     */
    fun isDPad(): Boolean =
        when (abstractSetting.key) {
            Settings.KEY_DPAD_AXIS_HORIZONTAL,
            Settings.KEY_DPAD_AXIS_VERTICAL -> true

            else -> false
        }

    /**
     * Returns true if this key is for the 3DS L/R or ZL/ZR buttons. Note, these are not real
     * triggers on the 3DS, but we support them as such on a physical gamepad.
     */
    fun isTrigger(): Boolean =
        when (abstractSetting.key) {
            Settings.KEY_BUTTON_L,
            Settings.KEY_BUTTON_R,
            Settings.KEY_BUTTON_ZL,
            Settings.KEY_BUTTON_ZR -> true

            else -> false
        }

    /**
     * Returns true if a gamepad axis can be used to map this key.
     */
    fun isAxisMappingSupported(): Boolean {
        return isCirclePad() || isCStick() || isDPad() || isTrigger()
    }

    /**
     * Returns true if a gamepad button can be used to map this key.
     */
    fun isButtonMappingSupported(): Boolean {
        return !isAxisMappingSupported() || isTrigger()
    }

    /**
     * Returns the Citra button code for the settings key.
     */
    private val buttonCode: Int
        get() =
            when (abstractSetting.key) {
                Settings.KEY_BUTTON_A -> NativeLibrary.ButtonType.BUTTON_A
                Settings.KEY_BUTTON_B -> NativeLibrary.ButtonType.BUTTON_B
                Settings.KEY_BUTTON_X -> NativeLibrary.ButtonType.BUTTON_X
                Settings.KEY_BUTTON_Y -> NativeLibrary.ButtonType.BUTTON_Y
                Settings.KEY_BUTTON_L -> NativeLibrary.ButtonType.TRIGGER_L
                Settings.KEY_BUTTON_R -> NativeLibrary.ButtonType.TRIGGER_R
                Settings.KEY_BUTTON_ZL -> NativeLibrary.ButtonType.BUTTON_ZL
                Settings.KEY_BUTTON_ZR -> NativeLibrary.ButtonType.BUTTON_ZR
                Settings.KEY_BUTTON_SELECT -> NativeLibrary.ButtonType.BUTTON_SELECT
                Settings.KEY_BUTTON_START -> NativeLibrary.ButtonType.BUTTON_START
                Settings.KEY_BUTTON_HOME -> NativeLibrary.ButtonType.BUTTON_HOME
                Settings.KEY_BUTTON_UP -> NativeLibrary.ButtonType.DPAD_UP
                Settings.KEY_BUTTON_DOWN -> NativeLibrary.ButtonType.DPAD_DOWN
                Settings.KEY_BUTTON_LEFT -> NativeLibrary.ButtonType.DPAD_LEFT
                Settings.KEY_BUTTON_RIGHT -> NativeLibrary.ButtonType.DPAD_RIGHT

                Settings.HOTKEY_SCREEN_SWAP -> Hotkey.SWAP_SCREEN.button
                Settings.HOTKEY_CYCLE_LAYOUT -> Hotkey.CYCLE_LAYOUT.button
                Settings.HOTKEY_CLOSE_GAME -> Hotkey.CLOSE_GAME.button
                Settings.HOTKEY_PAUSE_OR_RESUME -> Hotkey.PAUSE_OR_RESUME.button
                Settings.HOTKEY_QUICKSAVE -> Hotkey.QUICKSAVE.button
                Settings.HOTKEY_QUICKlOAD -> Hotkey.QUICKLOAD.button
                else -> -1
            }

    /**
     * Returns the key used to lookup the reverse mapping for this key, which is used to cleanup old
     * settings on re-mapping or clearing of a setting.
     */
    private val reverseKey: String
        get() {
            var reverseKey = "${INPUT_MAPPING_PREFIX}_ReverseMapping_${abstractSetting.key}"
            if (isAxisMappingSupported() && !isTrigger()) {
                // Triggers are the only axis-supported mappings without orientation
                reverseKey += "_" + if (isHorizontalOrientation()) {
                    0
                } else {
                    1
                }
            }
            return reverseKey
        }

    /**
     * Removes the old mapping for this key from the settings, e.g. on user clearing the setting.
     */
    fun removeOldMapping() {
        // Try remove all possible keys we wrote for this setting
        val oldKey = preferences.getString(reverseKey, "")
        if (oldKey != "") {
            preferences.edit()
                .remove(abstractSetting.key) // Used for ui text
                .remove(oldKey) // Used for button mapping
                .remove(oldKey + "_GuestOrientation") // Used for axis orientation
                .remove(oldKey + "_GuestButton") // Used for axis button
                .apply()
        }
    }

    /**
     * Helper function to write a gamepad button mapping for the setting.
     */
    private fun writeButtonMapping(key: String) {
        val editor = preferences.edit()

        // Remove mapping for another setting using this input
        val oldButtonCode = preferences.getInt(key, -1)
        if (oldButtonCode != -1) {
            val oldKey = getButtonKey(oldButtonCode)
            editor.remove(oldKey) // Only need to remove UI text setting, others will be overwritten
        }

        // Cleanup old mapping for this setting
        removeOldMapping()

        // Write new mapping
        editor.putInt(key, buttonCode)

        // Write next reverse mapping for future cleanup
        editor.putString(reverseKey, key)

        // Apply changes
        editor.apply()
    }

    /**
     * Helper function to write a gamepad axis mapping for the setting.
     */
    private fun writeAxisMapping(axis: Int, value: Int) {
        // Cleanup old mapping
        removeOldMapping()

        // Write new mapping
        preferences.edit()
            .putInt(getInputAxisOrientationKey(axis), if (isHorizontalOrientation()) 0 else 1)
            .putInt(getInputAxisButtonKey(axis), value)
            // Write next reverse mapping for future cleanup
            .putString(reverseKey, getInputAxisKey(axis))
            .apply()
    }

    /**
     * Saves the provided key input setting as an Android preference.
     *
     * @param keyEvent KeyEvent of this key press.
     */
    fun onKeyInput(keyEvent: KeyEvent) {
        if (!isButtonMappingSupported()) {
            Toast.makeText(context, R.string.input_message_analog_only, Toast.LENGTH_LONG).show()
            return
        }

        val code = translateEventToKeyId(keyEvent)
        writeButtonMapping(getInputButtonKey(code))
        val uiString = "${keyEvent.device.name}: Button $code"
        value = uiString
    }

    /**
     * Saves the provided motion input setting as an Android preference.
     *
     * @param device      InputDevice from which the input event originated.
     * @param motionRange MotionRange of the movement
     * @param axisDir     Either '-' or '+' (currently unused)
     */
    fun onMotionInput(device: InputDevice, motionRange: MotionRange, axisDir: Char) {
        if (!isAxisMappingSupported()) {
            Toast.makeText(context, R.string.input_message_button_only, Toast.LENGTH_LONG).show()
            return
        }
        val button = if (isCirclePad()) {
            NativeLibrary.ButtonType.STICK_LEFT
        } else if (isCStick()) {
            NativeLibrary.ButtonType.STICK_C
        } else if (isDPad()) {
            NativeLibrary.ButtonType.DPAD
        } else {
            buttonCode
        }
        writeAxisMapping(motionRange.axis, button)
        val uiString = "${device.name}: Axis ${motionRange.axis}"
        value = uiString
    }

    override val type = TYPE_INPUT_BINDING

    companion object {
        private const val INPUT_MAPPING_PREFIX = "InputMapping"

        /**
         * Returns the settings key for the specified Citra button code.
         */
        private fun getButtonKey(buttonCode: Int): String =
            when (buttonCode) {
                NativeLibrary.ButtonType.BUTTON_A -> Settings.KEY_BUTTON_A
                NativeLibrary.ButtonType.BUTTON_B -> Settings.KEY_BUTTON_B
                NativeLibrary.ButtonType.BUTTON_X -> Settings.KEY_BUTTON_X
                NativeLibrary.ButtonType.BUTTON_Y -> Settings.KEY_BUTTON_Y
                NativeLibrary.ButtonType.TRIGGER_L -> Settings.KEY_BUTTON_L
                NativeLibrary.ButtonType.TRIGGER_R -> Settings.KEY_BUTTON_R
                NativeLibrary.ButtonType.BUTTON_ZL -> Settings.KEY_BUTTON_ZL
                NativeLibrary.ButtonType.BUTTON_ZR -> Settings.KEY_BUTTON_ZR
                NativeLibrary.ButtonType.BUTTON_SELECT -> Settings.KEY_BUTTON_SELECT
                NativeLibrary.ButtonType.BUTTON_START -> Settings.KEY_BUTTON_START
                NativeLibrary.ButtonType.BUTTON_HOME -> Settings.KEY_BUTTON_HOME
                NativeLibrary.ButtonType.DPAD_UP -> Settings.KEY_BUTTON_UP
                NativeLibrary.ButtonType.DPAD_DOWN -> Settings.KEY_BUTTON_DOWN
                NativeLibrary.ButtonType.DPAD_LEFT -> Settings.KEY_BUTTON_LEFT
                NativeLibrary.ButtonType.DPAD_RIGHT -> Settings.KEY_BUTTON_RIGHT
                else -> ""
            }

        /**
         * Helper function to get the settings key for an gamepad button.
         *
         */
        @Deprecated("Use the new getInputButtonKey(keyEvent) method to handle unknown keys")
        fun getInputButtonKey(keyCode: Int): String = "${INPUT_MAPPING_PREFIX}_HostAxis_${keyCode}"

        /**
         * Helper function to get the settings key for an gamepad button.
         *
         */
        fun getInputButtonKey(event: KeyEvent): String = "${INPUT_MAPPING_PREFIX}_HostAxis_${translateEventToKeyId(event)}"

        /**
         * Helper function to get the settings key for an gamepad axis.
         */
        fun getInputAxisKey(axis: Int): String = "${INPUT_MAPPING_PREFIX}_HostAxis_${axis}"

        /**
         * Helper function to get the settings key for an gamepad axis button (stick or trigger).
         */
        fun getInputAxisButtonKey(axis: Int): String = "${getInputAxisKey(axis)}_GuestButton"

        /**
         * Helper function to get the settings key for an gamepad axis orientation.
         */
        fun getInputAxisOrientationKey(axis: Int): String =
            "${getInputAxisKey(axis)}_GuestOrientation"


        /**
         * This function translates a keyEvent into an "keyid"
         * This key id is either the keyCode from the event, or
         * the raw scanCode.
         * Only when the keyCode itself is 0, (so it is an unknown key)
         * we fall back to the raw scan code.
         * This handles keys like the media-keys on google statia-controllers
         * that don't have a conventional "mapping" and report as "unknown"
         */
        fun translateEventToKeyId(event: KeyEvent): Int {
            return if (event.keyCode == 0) {
                event.scanCode
            } else {
                event.keyCode
            }
        }
    }
}
