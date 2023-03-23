package org.citra.citra_emu.utils;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.widget.TextView;
import androidx.activity.result.ActivityResultLauncher;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentActivity;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;

public final class StartupHandler {
    private static void handlePermissionsCheck(FragmentActivity parent,
                                               ActivityResultLauncher<Uri> launcher) {
        // Ask the user to grant write permission if it's not already granted
        PermissionsHandler.checkWritePermission(parent, launcher);

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

    public static void HandleInit(FragmentActivity parent, ActivityResultLauncher<Uri> launcher) {
        if (PermissionsHandler.isFirstBoot(parent)) {
            // Prompt user with standard first boot disclaimer
            AlertDialog dialog =
                new MaterialAlertDialogBuilder(parent)
                    .setTitle(R.string.app_name)
                    .setIcon(R.mipmap.ic_launcher)
                    .setMessage(R.string.app_disclaimer)
                    .setPositiveButton(android.R.string.ok, null)
                    .setCancelable(false)
                    .setOnDismissListener(
                        dialogInterface -> handlePermissionsCheck(parent, launcher))
                    .show();
            TextView textView = dialog.findViewById(android.R.id.message);
            if (textView == null)
                return;
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        }
    }
}
