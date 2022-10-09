package org.citra.citra_emu.features.settings.utils;

import androidx.annotation.NonNull;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.features.settings.model.FloatSetting;
import org.citra.citra_emu.features.settings.model.IntSetting;
import org.citra.citra_emu.features.settings.model.Setting;
import org.citra.citra_emu.features.settings.model.SettingSection;
import org.citra.citra_emu.features.settings.model.Settings;
import org.citra.citra_emu.features.settings.model.StringSetting;
import org.citra.citra_emu.features.settings.ui.SettingsActivityView;
import org.citra.citra_emu.utils.BiMap;
import org.citra.citra_emu.utils.DirectoryInitialization;
import org.citra.citra_emu.utils.Log;
import org.ini4j.Wini;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
public final class SettingsFile {
    public static final String FILE_NAME_CONFIG = "config";

    public static final String KEY_CPU_JIT = "use_cpu_jit";

    public static final String KEY_DESIGN = "design";

    public static final String KEY_PREMIUM = "premium";

    public static final String KEY_HW_RENDERER = "use_hw_renderer";
    public static final String KEY_HW_SHADER = "use_hw_shader";
    public static final String KEY_SHADERS_ACCURATE_MUL = "shaders_accurate_mul";
    public static final String KEY_USE_SHADER_JIT = "use_shader_jit";
    public static final String KEY_USE_DISK_SHADER_CACHE = "use_disk_shader_cache";
    public static final String KEY_USE_VSYNC = "use_vsync_new";
    public static final String KEY_RESOLUTION_FACTOR = "resolution_factor";
    public static final String KEY_FRAME_LIMIT_ENABLED = "use_frame_limit";
    public static final String KEY_FRAME_LIMIT = "frame_limit";
    public static final String KEY_BACKGROUND_RED = "bg_red";
    public static final String KEY_BACKGROUND_BLUE = "bg_blue";
    public static final String KEY_BACKGROUND_GREEN = "bg_green";
    public static final String KEY_RENDER_3D = "render_3d";
    public static final String KEY_FACTOR_3D = "factor_3d";
    public static final String KEY_PP_SHADER_NAME = "pp_shader_name";
    public static final String KEY_FILTER_MODE = "filter_mode";
    public static final String KEY_TEXTURE_FILTER_NAME = "texture_filter_name";
    public static final String KEY_USE_ASYNCHRONOUS_GPU_EMULATION = "use_asynchronous_gpu_emulation";

    public static final String KEY_LAYOUT_OPTION = "layout_option";
    public static final String KEY_SWAP_SCREEN = "swap_screen";
    public static final String KEY_CARDBOARD_SCREEN_SIZE = "cardboard_screen_size";
    public static final String KEY_CARDBOARD_X_SHIFT = "cardboard_x_shift";
    public static final String KEY_CARDBOARD_Y_SHIFT = "cardboard_y_shift";

    public static final String KEY_DUMP_TEXTURES = "dump_textures";
    public static final String KEY_CUSTOM_TEXTURES = "custom_textures";
    public static final String KEY_PRELOAD_TEXTURES = "preload_textures";

    public static final String KEY_AUDIO_OUTPUT_ENGINE = "output_engine";
    public static final String KEY_ENABLE_AUDIO_STRETCHING = "enable_audio_stretching";
    public static final String KEY_VOLUME = "volume";
    public static final String KEY_MIC_INPUT_TYPE = "mic_input_type";

    public static final String KEY_USE_VIRTUAL_SD = "use_virtual_sd";

    public static final String KEY_IS_NEW_3DS = "is_new_3ds";
    public static final String KEY_REGION_VALUE = "region_value";
    public static final String KEY_LANGUAGE = "language";

    public static final String KEY_INIT_CLOCK = "init_clock";
    public static final String KEY_INIT_TIME = "init_time";

    public static final String KEY_BUTTON_A = "button_a";
    public static final String KEY_BUTTON_B = "button_b";
    public static final String KEY_BUTTON_X = "button_x";
    public static final String KEY_BUTTON_Y = "button_y";
    public static final String KEY_BUTTON_SELECT = "button_select";
    public static final String KEY_BUTTON_START = "button_start";
    public static final String KEY_BUTTON_UP = "button_up";
    public static final String KEY_BUTTON_DOWN = "button_down";
    public static final String KEY_BUTTON_LEFT = "button_left";
    public static final String KEY_BUTTON_RIGHT = "button_right";
    public static final String KEY_BUTTON_L = "button_l";
    public static final String KEY_BUTTON_R = "button_r";
    public static final String KEY_BUTTON_ZL = "button_zl";
    public static final String KEY_BUTTON_ZR = "button_zr";
    public static final String KEY_CIRCLEPAD_AXIS_VERTICAL = "circlepad_axis_vertical";
    public static final String KEY_CIRCLEPAD_AXIS_HORIZONTAL = "circlepad_axis_horizontal";
    public static final String KEY_CSTICK_AXIS_VERTICAL = "cstick_axis_vertical";
    public static final String KEY_CSTICK_AXIS_HORIZONTAL = "cstick_axis_horizontal";
    public static final String KEY_DPAD_AXIS_VERTICAL = "dpad_axis_vertical";
    public static final String KEY_DPAD_AXIS_HORIZONTAL = "dpad_axis_horizontal";
    public static final String KEY_CIRCLEPAD_UP = "circlepad_up";
    public static final String KEY_CIRCLEPAD_DOWN = "circlepad_down";
    public static final String KEY_CIRCLEPAD_LEFT = "circlepad_left";
    public static final String KEY_CIRCLEPAD_RIGHT = "circlepad_right";
    public static final String KEY_CSTICK_UP = "cstick_up";
    public static final String KEY_CSTICK_DOWN = "cstick_down";
    public static final String KEY_CSTICK_LEFT = "cstick_left";
    public static final String KEY_CSTICK_RIGHT = "cstick_right";

    public static final String KEY_CAMERA_OUTER_RIGHT_NAME = "camera_outer_right_name";
    public static final String KEY_CAMERA_OUTER_RIGHT_CONFIG = "camera_outer_right_config";
    public static final String KEY_CAMERA_OUTER_RIGHT_FLIP = "camera_outer_right_flip";
    public static final String KEY_CAMERA_OUTER_LEFT_NAME = "camera_outer_left_name";
    public static final String KEY_CAMERA_OUTER_LEFT_CONFIG = "camera_outer_left_config";
    public static final String KEY_CAMERA_OUTER_LEFT_FLIP = "camera_outer_left_flip";
    public static final String KEY_CAMERA_INNER_NAME = "camera_inner_name";
    public static final String KEY_CAMERA_INNER_CONFIG = "camera_inner_config";
    public static final String KEY_CAMERA_INNER_FLIP = "camera_inner_flip";

    public static final String KEY_LOG_FILTER = "log_filter";

    private static BiMap<String, String> sectionsMap = new BiMap<>();

    static {
        //TODO: Add members to sectionsMap when game-specific settings are added
    }


    private SettingsFile() {
    }

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of Settings, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param ini          The ini file to load the settings from
     * @param isCustomGame
     * @param view         The current view.
     * @return An Observable that emits a HashMap of the file's contents, then completes.
     */
    static HashMap<String, SettingSection> readFile(final File ini, boolean isCustomGame, SettingsActivityView view) {
        HashMap<String, SettingSection> sections = new Settings.SettingsSectionMap();

        BufferedReader reader = null;

        try {
            reader = new BufferedReader(new FileReader(ini));

            SettingSection current = null;
            for (String line; (line = reader.readLine()) != null; ) {
                if (line.startsWith("[") && line.endsWith("]")) {
                    current = sectionFromLine(line, isCustomGame);
                    sections.put(current.getName(), current);
                } else if ((current != null)) {
                    Setting setting = settingFromLine(current, line);
                    if (setting != null) {
                        current.putSetting(setting);
                    }
                }
            }
        } catch (FileNotFoundException e) {
            Log.error("[SettingsFile] File not found: " + ini.getAbsolutePath() + e.getMessage());
            if (view != null)
                view.onSettingsFileNotFound();
        } catch (IOException e) {
            Log.error("[SettingsFile] Error reading from: " + ini.getAbsolutePath() + e.getMessage());
            if (view != null)
                view.onSettingsFileNotFound();
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e) {
                    Log.error("[SettingsFile] Error closing: " + ini.getAbsolutePath() + e.getMessage());
                }
            }
        }

        return sections;
    }

    public static HashMap<String, SettingSection> readFile(final String fileName, SettingsActivityView view) {
        return readFile(getSettingsFile(fileName), false, view);
    }

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of SettingSections, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param gameId the id of the game to load it's settings.
     * @param view   The current view.
     */
    public static HashMap<String, SettingSection> readCustomGameSettings(final String gameId, SettingsActivityView view) {
        return readFile(getCustomGameSettingsFile(gameId), true, view);
    }

    /**
     * Saves a Settings HashMap to a given .ini file on disk. If unsuccessful, outputs an error
     * telling why it failed.
     *
     * @param fileName The target filename without a path or extension.
     * @param sections The HashMap containing the Settings we want to serialize.
     * @param view     The current view.
     */
    public static void saveFile(final String fileName, TreeMap<String, SettingSection> sections,
                                SettingsActivityView view) {
        File ini = getSettingsFile(fileName);

        try {
            Wini writer = new Wini(ini);

            Set<String> keySet = sections.keySet();
            for (String key : keySet) {
                SettingSection section = sections.get(key);
                writeSection(writer, section);
            }
            writer.store();
        } catch (IOException e) {
            Log.error("[SettingsFile] File not found: " + fileName + ".ini: " + e.getMessage());
            view.showToastMessage(CitraApplication.getAppContext().getString(R.string.error_saving, fileName, e.getMessage()), false);
        }
    }


    public static void saveCustomGameSettings(final String gameId, final HashMap<String, SettingSection> sections) {
        Set<String> sortedSections = new TreeSet<>(sections.keySet());

        for (String sectionKey : sortedSections) {
            SettingSection section = sections.get(sectionKey);

            HashMap<String, Setting> settings = section.getSettings();
            Set<String> sortedKeySet = new TreeSet<>(settings.keySet());

            for (String settingKey : sortedKeySet) {
                Setting setting = settings.get(settingKey);
                NativeLibrary.SetUserSetting(gameId, mapSectionNameFromIni(section.getName()), setting.getKey(), setting.getValueAsString());
            }
        }
    }

    private static String mapSectionNameFromIni(String generalSectionName) {
        if (sectionsMap.getForward(generalSectionName) != null) {
            return sectionsMap.getForward(generalSectionName);
        }

        return generalSectionName;
    }

    private static String mapSectionNameToIni(String generalSectionName) {
        if (sectionsMap.getBackward(generalSectionName) != null) {
            return sectionsMap.getBackward(generalSectionName);
        }

        return generalSectionName;
    }

    @NonNull
    private static File getSettingsFile(String fileName) {
        return new File(
                DirectoryInitialization.getUserDirectory() + "/config/" + fileName + ".ini");
    }

    private static File getCustomGameSettingsFile(String gameId) {
        return new File(DirectoryInitialization.getUserDirectory() + "/GameSettings/" + gameId + ".ini");
    }

    private static SettingSection sectionFromLine(String line, boolean isCustomGame) {
        String sectionName = line.substring(1, line.length() - 1);
        if (isCustomGame) {
            sectionName = mapSectionNameToIni(sectionName);
        }
        return new SettingSection(sectionName);
    }

    /**
     * For a line of text, determines what type of data is being represented, and returns
     * a Setting object containing this data.
     *
     * @param current The section currently being parsed by the consuming method.
     * @param line    The line of text being parsed.
     * @return A typed Setting containing the key/value contained in the line.
     */
    private static Setting settingFromLine(SettingSection current, String line) {
        String[] splitLine = line.split("=");

        if (splitLine.length != 2) {
            Log.warning("Skipping invalid config line \"" + line + "\"");
            return null;
        }

        String key = splitLine[0].trim();
        String value = splitLine[1].trim();

        if (value.isEmpty()) {
            Log.warning("Skipping null value in config line \"" + line + "\"");
            return null;
        }

        try {
            int valueAsInt = Integer.parseInt(value);

            return new IntSetting(key, current.getName(), valueAsInt);
        } catch (NumberFormatException ex) {
        }

        try {
            float valueAsFloat = Float.parseFloat(value);

            return new FloatSetting(key, current.getName(), valueAsFloat);
        } catch (NumberFormatException ex) {
        }

        return new StringSetting(key, current.getName(), value);
    }

    /**
     * Writes the contents of a Section HashMap to disk.
     *
     * @param parser  A Wini pointed at a file on disk.
     * @param section A section containing settings to be written to the file.
     */
    private static void writeSection(Wini parser, SettingSection section) {
        // Write the section header.
        String header = section.getName();

        // Write this section's values.
        HashMap<String, Setting> settings = section.getSettings();
        Set<String> keySet = settings.keySet();

        for (String key : keySet) {
            Setting setting = settings.get(key);
            parser.put(header, setting.getKey(), setting.getValueAsString());
        }
    }
}
