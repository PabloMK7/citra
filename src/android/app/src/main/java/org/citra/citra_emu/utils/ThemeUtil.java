package org.citra.citra_emu.utils;

import android.app.Activity;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.preference.PreferenceManager;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.content.ContextCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.google.android.material.color.MaterialColors;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.R;
import org.citra.citra_emu.features.settings.utils.SettingsFile;

public class ThemeUtil {
    private static SharedPreferences mPreferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());

    public static final float NAV_BAR_ALPHA = 0.9f;

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

        setSystemBarMode(activity, getIsLightMode(activity.getResources()));
        setNavigationBarColor(activity, MaterialColors.getColor(activity.getWindow().getDecorView(), R.attr.colorSurface));
    }

    public static void applyTheme(AppCompatActivity activity) {
        applyTheme(mPreferences.getInt(SettingsFile.KEY_DESIGN, 0), activity);
    }

    public static void setSystemBarMode(AppCompatActivity activity, boolean isLightMode) {
        WindowInsetsControllerCompat windowController = WindowCompat.getInsetsController(activity.getWindow(), activity.getWindow().getDecorView());
        windowController.setAppearanceLightStatusBars(isLightMode);
        windowController.setAppearanceLightNavigationBars(isLightMode);
    }

    public static void setNavigationBarColor(@NonNull Activity activity, @ColorInt int color) {
        int gestureType = InsetsHelper.getSystemGestureType(activity.getApplicationContext());
        int orientation = activity.getResources().getConfiguration().orientation;

        // Use a solid color when the navigation bar is on the left/right edge of the screen
        if ((gestureType == InsetsHelper.THREE_BUTTON_NAVIGATION ||
                gestureType == InsetsHelper.TWO_BUTTON_NAVIGATION) &&
                orientation == Configuration.ORIENTATION_LANDSCAPE) {
            activity.getWindow().setNavigationBarColor(color);
        } else if (gestureType == InsetsHelper.THREE_BUTTON_NAVIGATION ||
                gestureType == InsetsHelper.TWO_BUTTON_NAVIGATION) {
            // Use semi-transparent color when in portrait mode with three/two button navigation to
            // partially see list items behind the navigation bar
            activity.getWindow().setNavigationBarColor(ThemeUtil.getColorWithOpacity(color, NAV_BAR_ALPHA));
        } else {
            // Use transparent color when using gesture navigation
            activity.getWindow().setNavigationBarColor(
                    ContextCompat.getColor(activity.getApplicationContext(),
                            android.R.color.transparent));
        }
    }

    @ColorInt
    public static int getColorWithOpacity(@ColorInt int color, float alphaFactor) {
        return Color.argb(Math.round(alphaFactor * Color.alpha(color)), Color.red(color),
                Color.green(color), Color.blue(color));
    }

    public static boolean getIsLightMode(Resources resources) {
        return (resources.getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_NO;
    }
}
