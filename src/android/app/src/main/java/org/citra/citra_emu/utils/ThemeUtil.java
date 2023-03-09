package org.citra.citra_emu.utils;

import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Build;
import android.preference.PreferenceManager;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.features.settings.utils.SettingsFile;

public class ThemeUtil {
    private static SharedPreferences mPreferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());

    private static void applyTheme(int designValue, AppCompatActivity activity) {
        switch (designValue) {
            case 0:
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
                break;
            case 1:
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);
                break;
            case 2:
                AppCompatDelegate.setDefaultNightMode(android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q ?
                        AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM :
                        AppCompatDelegate.MODE_NIGHT_AUTO_BATTERY);
                break;
        }

        int systemReportedThemeMode = activity.getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        WindowInsetsControllerCompat windowController = WindowCompat.getInsetsController(activity.getWindow(), activity.getWindow().getDecorView());
        windowController.setAppearanceLightStatusBars(systemReportedThemeMode == Configuration.UI_MODE_NIGHT_NO);
    }

    public static void applyTheme(AppCompatActivity activity) {
        applyTheme(mPreferences.getInt(SettingsFile.KEY_DESIGN, 0), activity);
    }
}
