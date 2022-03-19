package org.citra.citra_emu.features.settings.model;

import android.text.TextUtils;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.R;
import org.citra.citra_emu.features.settings.ui.SettingsActivityView;
import org.citra.citra_emu.features.settings.utils.SettingsFile;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

public class Settings {
    public static final String SECTION_PREMIUM = "Premium";
    public static final String SECTION_CORE = "Core";
    public static final String SECTION_SYSTEM = "System";
    public static final String SECTION_CAMERA = "Camera";
    public static final String SECTION_CONTROLS = "Controls";
    public static final String SECTION_RENDERER = "Renderer";
    public static final String SECTION_LAYOUT = "Layout";
    public static final String SECTION_AUDIO = "Audio";
    public static final String SECTION_DEBUG = "Debug";

    private String gameId;

    private static final Map<String, List<String>> configFileSectionsMap = new HashMap<>();

    static {
        configFileSectionsMap.put(SettingsFile.FILE_NAME_CONFIG, Arrays.asList(SECTION_PREMIUM, SECTION_CORE, SECTION_SYSTEM, SECTION_CAMERA, SECTION_CONTROLS, SECTION_RENDERER, SECTION_LAYOUT, SECTION_AUDIO, SECTION_DEBUG));
    }

    /**
     * A HashMap<String, SettingSection> that constructs a new SettingSection instead of returning null
     * when getting a key not already in the map
     */
    public static final class SettingsSectionMap extends HashMap<String, SettingSection> {
        @Override
        public SettingSection get(Object key) {
            if (!(key instanceof String)) {
                return null;
            }

            String stringKey = (String) key;

            if (!super.containsKey(stringKey)) {
                SettingSection section = new SettingSection(stringKey);
                super.put(stringKey, section);
                return section;
            }
            return super.get(key);
        }
    }

    private HashMap<String, SettingSection> sections = new Settings.SettingsSectionMap();

    public SettingSection getSection(String sectionName) {
        return sections.get(sectionName);
    }

    public boolean isEmpty() {
        return sections.isEmpty();
    }

    public HashMap<String, SettingSection> getSections() {
        return sections;
    }

    public void loadSettings(SettingsActivityView view) {
        sections = new Settings.SettingsSectionMap();
        loadCitraSettings(view);

        if (!TextUtils.isEmpty(gameId)) {
            loadCustomGameSettings(gameId, view);
        }
    }

    private void loadCitraSettings(SettingsActivityView view) {
        for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet()) {
            String fileName = entry.getKey();
            sections.putAll(SettingsFile.readFile(fileName, view));
        }
    }

    private void loadCustomGameSettings(String gameId, SettingsActivityView view) {
        // custom game settings
        mergeSections(SettingsFile.readCustomGameSettings(gameId, view));
    }

    private void mergeSections(HashMap<String, SettingSection> updatedSections) {
        for (Map.Entry<String, SettingSection> entry : updatedSections.entrySet()) {
            if (sections.containsKey(entry.getKey())) {
                SettingSection originalSection = sections.get(entry.getKey());
                SettingSection updatedSection = entry.getValue();
                originalSection.mergeSection(updatedSection);
            } else {
                sections.put(entry.getKey(), entry.getValue());
            }
        }
    }

    public void loadSettings(String gameId, SettingsActivityView view) {
        this.gameId = gameId;
        loadSettings(view);
    }

    public void saveSettings(SettingsActivityView view) {
        if (TextUtils.isEmpty(gameId)) {
            view.showToastMessage(CitraApplication.getAppContext().getString(R.string.ini_saved), false);

            for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet()) {
                String fileName = entry.getKey();
                List<String> sectionNames = entry.getValue();
                TreeMap<String, SettingSection> iniSections = new TreeMap<>();
                for (String section : sectionNames) {
                    iniSections.put(section, sections.get(section));
                }

                SettingsFile.saveFile(fileName, iniSections, view);
            }
        } else {
            // custom game settings
            view.showToastMessage(CitraApplication.getAppContext().getString(R.string.gameid_saved, gameId), false);

            SettingsFile.saveCustomGameSettings(gameId, sections);
        }

    }
}