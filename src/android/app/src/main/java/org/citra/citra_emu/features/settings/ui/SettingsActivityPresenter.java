package org.citra.citra_emu.features.settings.ui;

import android.content.IntentFilter;
import android.os.Bundle;
import android.text.TextUtils;
import androidx.appcompat.app.AppCompatActivity;
import androidx.documentfile.provider.DocumentFile;
import java.io.File;
import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.features.settings.model.Settings;
import org.citra.citra_emu.features.settings.utils.SettingsFile;
import org.citra.citra_emu.utils.DirectoryInitialization;
import org.citra.citra_emu.utils.DirectoryInitialization.DirectoryInitializationState;
import org.citra.citra_emu.utils.Log;
import org.citra.citra_emu.utils.ThemeUtil;

public final class SettingsActivityPresenter {
    private static final String KEY_SHOULD_SAVE = "should_save";

    private SettingsActivityView mView;

    private Settings mSettings = new Settings();

    private boolean mShouldSave;

    private String menuTag;
    private String gameId;

    public SettingsActivityPresenter(SettingsActivityView view) {
        mView = view;
    }

    public void onCreate(Bundle savedInstanceState, String menuTag, String gameId) {
        if (savedInstanceState == null) {
            this.menuTag = menuTag;
            this.gameId = gameId;
        } else {
            mShouldSave = savedInstanceState.getBoolean(KEY_SHOULD_SAVE);
        }
    }

    public void onStart() {
        prepareCitraDirectoriesIfNeeded();
    }

    void loadSettingsUI() {
        if (mSettings.isEmpty()) {
            if (!TextUtils.isEmpty(gameId)) {
                mSettings.loadSettings(gameId, mView);
            } else {
                mSettings.loadSettings(mView);
            }
        }

        mView.showSettingsFragment(menuTag, false, gameId);
        mView.onSettingsFileLoaded(mSettings);
    }

    private void prepareCitraDirectoriesIfNeeded() {
        DocumentFile configFile = SettingsFile.getSettingsFile(SettingsFile.FILE_NAME_CONFIG);
        if (configFile == null || !configFile.exists()) {
            Log.error("Citra config file could not be found!");
        }
        loadSettingsUI();
    }

    public void setSettings(Settings settings) {
        mSettings = settings;
    }

    public Settings getSettings() {
        return mSettings;
    }

    public void onStop(boolean finishing) {
        if (mSettings != null && finishing && mShouldSave) {
            Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
            mSettings.saveSettings(mView);
        }

        NativeLibrary.INSTANCE.reloadSettings();
    }

    public void onSettingChanged() {
        mShouldSave = true;
    }

    public void saveState(Bundle outState) {
        outState.putBoolean(KEY_SHOULD_SAVE, mShouldSave);
    }
}
