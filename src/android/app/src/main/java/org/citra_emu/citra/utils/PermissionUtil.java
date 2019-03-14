// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra_emu.citra.utils;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;

public class PermissionUtil {

    /**
     * Checks a permission, if needed shows a dialog to request it
     *
     * @param activity    the activity requiring the permission
     * @param permission  the permission needed
     * @param requestCode supplied to the callback to determine the next action
     */
    public static void verifyPermission(Activity activity, String permission, int requestCode) {
        if (ContextCompat.checkSelfPermission(activity, permission) ==
            PackageManager.PERMISSION_GRANTED) {
            // call the callback called by requestPermissions
            activity.onRequestPermissionsResult(requestCode, new String[] {permission},
                                                new int[] {PackageManager.PERMISSION_GRANTED});
            return;
        }

        ActivityCompat.requestPermissions(activity, new String[] {permission}, requestCode);
    }
}
