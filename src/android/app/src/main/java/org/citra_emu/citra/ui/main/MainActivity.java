// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra_emu.citra.ui.main;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;

import org.citra_emu.citra.R;
import org.citra_emu.citra.utils.FileUtil;
import org.citra_emu.citra.utils.PermissionUtil;

public final class MainActivity extends AppCompatActivity {

    // Java enums suck
    private interface PermissionCodes { int INITIALIZE = 0; }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        PermissionUtil.verifyPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE,
                                        PermissionCodes.INITIALIZE);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        switch (requestCode) {
        case PermissionCodes.INITIALIZE:
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                initUserPath(FileUtil.getUserPath().toString());
                initLogging();
            } else {
                AlertDialog.Builder dialog =
                    new AlertDialog.Builder(this)
                        .setTitle("Permission Error")
                        .setMessage("Citra requires storage permissions to function.")
                        .setCancelable(false)
                        .setPositiveButton("OK", (dialogInterface, which) -> {
                            PermissionUtil.verifyPermission(
                                MainActivity.this, Manifest.permission.WRITE_EXTERNAL_STORAGE,
                                PermissionCodes.INITIALIZE);
                        });
                dialog.show();
            }
        }
    }

    private static native void initUserPath(String path);
    private static native void initLogging();
}
