package org.citra.citra_emu.features.settings.model.view;

import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.widget.Toast;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.features.settings.model.Setting;
import org.citra.citra_emu.features.settings.model.StringSetting;
import org.citra.citra_emu.features.settings.utils.SettingsFile;

public final class InputBindingSetting extends SettingsItem {
    private static final String INPUT_MAPPING_PREFIX = "InputMapping";

    public InputBindingSetting(String key, String section, int titleId, Setting setting) {
        super(key, section, setting, titleId, 0);
    }

    public String getValue() {
        if (getSetting() == null) {
            return "";
        }

        StringSetting setting = (StringSetting) getSetting();
        return setting.getValue();
    }

    /**
     * Returns true if this key is for the 3DS Circle Pad
     */
    private boolean IsCirclePad() {
        switch (getKey()) {
            case SettingsFile.KEY_CIRCLEPAD_AXIS_HORIZONTAL:
            case SettingsFile.KEY_CIRCLEPAD_AXIS_VERTICAL:
                return true;
        }
        return false;
    }

    /**
     * Returns true if this key is for a horizontal axis for a 3DS analog stick or D-pad
     */
    public boolean IsHorizontalOrientation() {
        switch (getKey()) {
            case SettingsFile.KEY_CIRCLEPAD_AXIS_HORIZONTAL:
            case SettingsFile.KEY_CSTICK_AXIS_HORIZONTAL:
            case SettingsFile.KEY_DPAD_AXIS_HORIZONTAL:
                return true;
        }
        return false;
    }

    /**
     * Returns true if this key is for the 3DS C-Stick
     */
    private boolean IsCStick() {
        switch (getKey()) {
            case SettingsFile.KEY_CSTICK_AXIS_HORIZONTAL:
            case SettingsFile.KEY_CSTICK_AXIS_VERTICAL:
                return true;
        }
        return false;
    }

    /**
     * Returns true if this key is for the 3DS D-Pad
     */
    private boolean IsDPad() {
        switch (getKey()) {
            case SettingsFile.KEY_DPAD_AXIS_HORIZONTAL:
            case SettingsFile.KEY_DPAD_AXIS_VERTICAL:
                return true;
        }
        return false;
    }

    /**
     * Returns true if this key is for the 3DS L/R or ZL/ZR buttons. Note, these are not real
     * triggers on the 3DS, but we support them as such on a physical gamepad.
     */
    public boolean IsTrigger() {
        switch (getKey()) {
            case SettingsFile.KEY_BUTTON_L:
            case SettingsFile.KEY_BUTTON_R:
            case SettingsFile.KEY_BUTTON_ZL:
            case SettingsFile.KEY_BUTTON_ZR:
                return true;
        }
        return false;
    }

    /**
     * Returns true if a gamepad axis can be used to map this key.
     */
    public boolean IsAxisMappingSupported() {
        return IsCirclePad() || IsCStick() || IsDPad() || IsTrigger();
    }

    /**
     * Returns true if a gamepad button can be used to map this key.
     */
    private boolean IsButtonMappingSupported() {
        return !IsAxisMappingSupported() || IsTrigger();
    }

    /**
     * Returns the Citra button code for the settings key.
     */
    private int getButtonCode() {
        switch (getKey()) {
            case SettingsFile.KEY_BUTTON_A:
                return NativeLibrary.ButtonType.BUTTON_A;
            case SettingsFile.KEY_BUTTON_B:
                return NativeLibrary.ButtonType.BUTTON_B;
            case SettingsFile.KEY_BUTTON_X:
                return NativeLibrary.ButtonType.BUTTON_X;
            case SettingsFile.KEY_BUTTON_Y:
                return NativeLibrary.ButtonType.BUTTON_Y;
            case SettingsFile.KEY_BUTTON_L:
                return NativeLibrary.ButtonType.TRIGGER_L;
            case SettingsFile.KEY_BUTTON_R:
                return NativeLibrary.ButtonType.TRIGGER_R;
            case SettingsFile.KEY_BUTTON_ZL:
                return NativeLibrary.ButtonType.BUTTON_ZL;
            case SettingsFile.KEY_BUTTON_ZR:
                return NativeLibrary.ButtonType.BUTTON_ZR;
            case SettingsFile.KEY_BUTTON_SELECT:
                return NativeLibrary.ButtonType.BUTTON_SELECT;
            case SettingsFile.KEY_BUTTON_START:
                return NativeLibrary.ButtonType.BUTTON_START;
            case SettingsFile.KEY_BUTTON_UP:
                return NativeLibrary.ButtonType.DPAD_UP;
            case SettingsFile.KEY_BUTTON_DOWN:
                return NativeLibrary.ButtonType.DPAD_DOWN;
            case SettingsFile.KEY_BUTTON_LEFT:
                return NativeLibrary.ButtonType.DPAD_LEFT;
            case SettingsFile.KEY_BUTTON_RIGHT:
                return NativeLibrary.ButtonType.DPAD_RIGHT;
        }
        return -1;
    }

    /**
     * Returns the settings key for the specified Citra button code.
     */
    private static String getButtonKey(int buttonCode) {
        switch (buttonCode) {
            case NativeLibrary.ButtonType.BUTTON_A:
                return SettingsFile.KEY_BUTTON_A;
            case NativeLibrary.ButtonType.BUTTON_B:
                return SettingsFile.KEY_BUTTON_B;
            case NativeLibrary.ButtonType.BUTTON_X:
                return SettingsFile.KEY_BUTTON_X;
            case NativeLibrary.ButtonType.BUTTON_Y:
                return SettingsFile.KEY_BUTTON_Y;
            case NativeLibrary.ButtonType.TRIGGER_L:
                return SettingsFile.KEY_BUTTON_L;
            case NativeLibrary.ButtonType.TRIGGER_R:
                return SettingsFile.KEY_BUTTON_R;
            case NativeLibrary.ButtonType.BUTTON_ZL:
                return SettingsFile.KEY_BUTTON_ZL;
            case NativeLibrary.ButtonType.BUTTON_ZR:
                return SettingsFile.KEY_BUTTON_ZR;
            case NativeLibrary.ButtonType.BUTTON_SELECT:
                return SettingsFile.KEY_BUTTON_SELECT;
            case NativeLibrary.ButtonType.BUTTON_START:
                return SettingsFile.KEY_BUTTON_START;
            case NativeLibrary.ButtonType.DPAD_UP:
                return SettingsFile.KEY_BUTTON_UP;
            case NativeLibrary.ButtonType.DPAD_DOWN:
                return SettingsFile.KEY_BUTTON_DOWN;
            case NativeLibrary.ButtonType.DPAD_LEFT:
                return SettingsFile.KEY_BUTTON_LEFT;
            case NativeLibrary.ButtonType.DPAD_RIGHT:
                return SettingsFile.KEY_BUTTON_RIGHT;
        }
        return "";
    }

    /**
     * Returns the key used to lookup the reverse mapping for this key, which is used to cleanup old
     * settings on re-mapping or clearing of a setting.
     */
    private String getReverseKey() {
        String reverseKey = INPUT_MAPPING_PREFIX + "_ReverseMapping_" + getKey();

        if (IsAxisMappingSupported() && !IsTrigger()) {
            // Triggers are the only axis-supported mappings without orientation
            reverseKey += "_" + (IsHorizontalOrientation() ? 0 : 1);
        }

        return reverseKey;
    }

    /**
     * Removes the old mapping for this key from the settings, e.g. on user clearing the setting.
     */
    public void removeOldMapping() {
        // Get preferences editor
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());
        SharedPreferences.Editor editor = preferences.edit();

        // Try remove all possible keys we wrote for this setting
        String oldKey = preferences.getString(getReverseKey(), "");
        if (!oldKey.equals("")) {
            editor.remove(getKey()); // Used for ui text
            editor.remove(oldKey); // Used for button mapping
            editor.remove(oldKey + "_GuestOrientation"); // Used for axis orientation
            editor.remove(oldKey + "_GuestButton"); // Used for axis button
        }

        // Apply changes
        editor.apply();
    }

    /**
     * Helper function to get the settings key for an gamepad button.
     */
    public static String getInputButtonKey(int keyCode) {
        return INPUT_MAPPING_PREFIX + "_Button_" + keyCode;
    }

    /**
     * Helper function to get the settings key for an gamepad axis.
     */
    public static String getInputAxisKey(int axis) {
        return INPUT_MAPPING_PREFIX + "_HostAxis_" + axis;
    }

    /**
     * Helper function to get the settings key for an gamepad axis button (stick or trigger).
     */
    public static String getInputAxisButtonKey(int axis) {
        return getInputAxisKey(axis) + "_GuestButton";
    }

    /**
     * Helper function to get the settings key for an gamepad axis orientation.
     */
    public static String getInputAxisOrientationKey(int axis) {
        return getInputAxisKey(axis) + "_GuestOrientation";
    }

    /**
     * Helper function to write a gamepad button mapping for the setting.
     */
    private void WriteButtonMapping(String key) {
        // Get preferences editor
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());
        SharedPreferences.Editor editor = preferences.edit();

        // Remove mapping for another setting using this input
        int oldButtonCode = preferences.getInt(key, -1);
        if (oldButtonCode != -1) {
            String oldKey = getButtonKey(oldButtonCode);
            editor.remove(oldKey); // Only need to remove UI text setting, others will be overwritten
        }

        // Cleanup old mapping for this setting
        removeOldMapping();

        // Write new mapping
        editor.putInt(key, getButtonCode());

        // Write next reverse mapping for future cleanup
        editor.putString(getReverseKey(), key);

        // Apply changes
        editor.apply();
    }

    /**
     * Helper function to write a gamepad axis mapping for the setting.
     */
    private void WriteAxisMapping(int axis, int value) {
        // Get preferences editor
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());
        SharedPreferences.Editor editor = preferences.edit();

        // Cleanup old mapping
        removeOldMapping();

        // Write new mapping
        editor.putInt(getInputAxisOrientationKey(axis), IsHorizontalOrientation() ? 0 : 1);
        editor.putInt(getInputAxisButtonKey(axis), value);

        // Write next reverse mapping for future cleanup
        editor.putString(getReverseKey(), getInputAxisKey(axis));

        // Apply changes
        editor.apply();
    }

    /**
     * Saves the provided key input setting as an Android preference.
     *
     * @param keyEvent KeyEvent of this key press.
     */
    public void onKeyInput(KeyEvent keyEvent) {
        if (!IsButtonMappingSupported()) {
            Toast.makeText(CitraApplication.getAppContext(), R.string.input_message_analog_only, Toast.LENGTH_LONG).show();
            return;
        }

        InputDevice device = keyEvent.getDevice();

        WriteButtonMapping(getInputButtonKey(keyEvent.getKeyCode()));

        String uiString = device.getName() + ": Button " + keyEvent.getKeyCode();
        setUiString(uiString);
    }

    /**
     * Saves the provided motion input setting as an Android preference.
     *
     * @param device      InputDevice from which the input event originated.
     * @param motionRange MotionRange of the movement
     * @param axisDir     Either '-' or '+' (currently unused)
     */
    public void onMotionInput(InputDevice device, InputDevice.MotionRange motionRange,
                              char axisDir) {
        if (!IsAxisMappingSupported()) {
            Toast.makeText(CitraApplication.getAppContext(), R.string.input_message_button_only, Toast.LENGTH_LONG).show();
            return;
        }

        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());
        SharedPreferences.Editor editor = preferences.edit();

        int button;
        if (IsCirclePad()) {
            button = NativeLibrary.ButtonType.STICK_LEFT;
        } else if (IsCStick()) {
            button = NativeLibrary.ButtonType.STICK_C;
        } else if (IsDPad()) {
            button = NativeLibrary.ButtonType.DPAD;
        } else {
            button = getButtonCode();
        }

        WriteAxisMapping(motionRange.getAxis(), button);

        String uiString = device.getName() + ": Axis " + motionRange.getAxis();
        setUiString(uiString);

        editor.apply();
    }

    /**
     * Sets the string to use in the configuration UI for the gamepad input.
     */
    private StringSetting setUiString(String ui) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());
        SharedPreferences.Editor editor = preferences.edit();

        if (getSetting() == null) {
            StringSetting setting = new StringSetting(getKey(), getSection(), "");
            setSetting(setting);

            editor.putString(setting.getKey(), ui);
            editor.apply();

            return setting;
        } else {
            StringSetting setting = (StringSetting) getSetting();

            editor.putString(setting.getKey(), ui);
            editor.apply();

            return null;
        }
    }

    @Override
    public int getType() {
        return TYPE_INPUT_BINDING;
    }
}
