package org.citra.citra_emu.utils;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.preference.PreferenceManager;

import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.Nullable;
import androidx.documentfile.provider.DocumentFile;
import androidx.fragment.app.FragmentActivity;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.R;

public class PermissionsHandler {
    public static final String CITRA_DIRECTORY = "CITRA_DIRECTORY";
    public static final SharedPreferences mPreferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());

    // We use permissions acceptance as an indicator if this is a first boot for the user.
    public static boolean isFirstBoot(FragmentActivity activity) {
        return !hasWriteAccess(activity.getApplicationContext());
    }

    public static boolean checkWritePermission(FragmentActivity activity,
                                               ActivityResultLauncher<Uri> launcher) {
        if (isFirstBoot(activity)) {
            launcher.launch(null);
            return false;
        }

        return true;
    }

    public static boolean hasWriteAccess(Context context) {
        try {
            Uri uri = getCitraDirectory();
            if (uri == null)
                return false;
            int takeFlags = (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            context.getContentResolver().takePersistableUriPermission(uri, takeFlags);
            DocumentFile root = DocumentFile.fromTreeUri(context, uri);
            if (root != null && root.exists()) return true;
            context.getContentResolver().releasePersistableUriPermission(uri, takeFlags);
        } catch (Exception e) {
            Log.error("[PermissionsHandler]: Cannot check citra data directory permission, error: " + e.getMessage());
        }
        return false;
    }

    @Nullable
    public static Uri getCitraDirectory() {
        String directoryString = mPreferences.getString(CITRA_DIRECTORY, "");
        if (directoryString.isEmpty()) {
            return null;
        }
        return Uri.parse(directoryString);
    }

    public static boolean setCitraDirectory(String uriString) {
        return mPreferences.edit().putString(CITRA_DIRECTORY, uriString).commit();
    }
}
