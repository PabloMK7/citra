package org.citra.citra_emu.utils;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentActivity;

import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;

public final class StartupHandler {
    private static void handlePermissionsCheck(FragmentActivity parent) {
        // Ask the user to grant write permission if it's not already granted
        PermissionsHandler.checkWritePermission(parent);

        String start_file = "";
        Bundle extras = parent.getIntent().getExtras();
        if (extras != null) {
            start_file = extras.getString("AutoStartFile");
        }

        if (!TextUtils.isEmpty(start_file)) {
            // Start the emulation activity, send the ISO passed in and finish the main activity
            Intent emulation_intent = new Intent(parent, EmulationActivity.class);
            emulation_intent.putExtra("SelectedGame", start_file);
            parent.startActivity(emulation_intent);
            parent.finish();
        }
    }

    public static void HandleInit(FragmentActivity parent) {
        if (PermissionsHandler.isFirstBoot(parent)) {
            // Prompt user with standard first boot disclaimer
            new AlertDialog.Builder(parent)
                    .setTitle(R.string.app_name)
                    .setIcon(R.mipmap.ic_launcher)
                    .setMessage(parent.getResources().getString(R.string.app_disclaimer))
                    .setPositiveButton(android.R.string.ok, null)
                    .setOnDismissListener(dialogInterface -> handlePermissionsCheck(parent))
                    .show();
        }
    }
}
