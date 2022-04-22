// Copyright 2021 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.disk_shader_cache;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;
import org.citra.citra_emu.utils.Log;

import java.util.Objects;

public class DiskShaderCacheProgress {

    // Equivalent to VideoCore::LoadCallbackStage
    public enum LoadCallbackStage {
        Prepare,
        Decompile,
        Build,
        Complete,
    }

    private static final Object finishLock = new Object();
    private static ProgressDialogFragment fragment;

    public static class ProgressDialogFragment extends DialogFragment {
        ProgressBar progressBar;
        TextView progressText;
        AlertDialog dialog;

        static ProgressDialogFragment newInstance(String title, String message) {
            ProgressDialogFragment frag = new ProgressDialogFragment();
            Bundle args = new Bundle();
            args.putString("title", title);
            args.putString("message", message);
            frag.setArguments(args);
            return frag;
        }

        @NonNull
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final Activity emulationActivity = Objects.requireNonNull(getActivity());

            final String title = Objects.requireNonNull(Objects.requireNonNull(getArguments()).getString("title"));
            final String message = Objects.requireNonNull(Objects.requireNonNull(getArguments()).getString("message"));

            LayoutInflater inflater = LayoutInflater.from(emulationActivity);
            View view = inflater.inflate(R.layout.dialog_progress_bar, null);

            progressBar = view.findViewById(R.id.progress_bar);
            progressText = view.findViewById(R.id.progress_text);
            progressText.setText("");

            setCancelable(false);
            setRetainInstance(true);

            AlertDialog.Builder builder = new AlertDialog.Builder(emulationActivity);
            builder.setTitle(title);
            builder.setMessage(message);
            builder.setView(view);
            builder.setNegativeButton(android.R.string.cancel, null);

            dialog = builder.create();
            dialog.create();

            dialog.getButton(DialogInterface.BUTTON_NEGATIVE).setOnClickListener((v) -> emulationActivity.onBackPressed());

            synchronized (finishLock) {
                finishLock.notifyAll();
            }

            return dialog;
        }

        private void onUpdateProgress(String msg, int progress, int max) {
            Objects.requireNonNull(getActivity()).runOnUiThread(() -> {
                progressBar.setProgress(progress);
                progressBar.setMax(max);
                progressText.setText(String.format("%d/%d", progress, max));
                dialog.setMessage(msg);
            });
        }
    }

    private static void prepareDialog() {
        NativeLibrary.sEmulationActivity.get().runOnUiThread(() -> {
            final EmulationActivity emulationActivity = NativeLibrary.sEmulationActivity.get();
            fragment = ProgressDialogFragment.newInstance(emulationActivity.getString(R.string.loading), emulationActivity.getString(R.string.preparing_shaders));
            fragment.show(emulationActivity.getSupportFragmentManager(), "diskShaders");
        });

        synchronized (finishLock) {
            try {
                finishLock.wait();
            } catch (Exception ignored) {
            }
        }
    }

    public static void loadProgress(LoadCallbackStage stage, int progress, int max) {
        final EmulationActivity emulationActivity = NativeLibrary.sEmulationActivity.get();
        if (emulationActivity == null) {
            Log.error("[DiskShaderCacheProgress] EmulationActivity not present");
            return;
        }

        switch (stage) {
            case Prepare:
                prepareDialog();
                break;
            case Decompile:
                fragment.onUpdateProgress(emulationActivity.getString(R.string.preparing_shaders), progress, max);
                break;
            case Build:
                fragment.onUpdateProgress(emulationActivity.getString(R.string.building_shaders), progress, max);
                break;
            case Complete:
                // Workaround for when dialog is dismissed when the app is in the background
                fragment.dismissAllowingStateLoss();
                break;
        }
    }
}
