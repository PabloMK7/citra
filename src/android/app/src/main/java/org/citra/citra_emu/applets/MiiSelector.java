// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.applets;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Objects;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

public final class MiiSelector {
    public static class MiiSelectorConfig implements java.io.Serializable {
        public boolean enable_cancel_button;
        public String title;
        public long initially_selected_mii_index;
        // List of Miis to display
        public String[] mii_names;
    }

    public static class MiiSelectorData {
        public long return_code;
        public int index;

        private MiiSelectorData(long return_code, int index) {
            this.return_code = return_code;
            this.index = index;
        }
    }

    public static class MiiSelectorDialogFragment extends DialogFragment {
        static MiiSelectorDialogFragment newInstance(MiiSelectorConfig config) {
            MiiSelectorDialogFragment frag = new MiiSelectorDialogFragment();
            Bundle args = new Bundle();
            args.putSerializable("config", config);
            frag.setArguments(args);
            return frag;
        }

        @NonNull
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final Activity emulationActivity = Objects.requireNonNull(getActivity());

            MiiSelectorConfig config =
                    Objects.requireNonNull((MiiSelectorConfig) Objects.requireNonNull(getArguments())
                            .getSerializable("config"));

            // Note: we intentionally leave out the Standard Mii in the native code so that
            // the string can get translated
            ArrayList<String> list = new ArrayList<>();
            list.add(emulationActivity.getString(R.string.standard_mii));
            list.addAll(Arrays.asList(config.mii_names));

            final int initialIndex = config.initially_selected_mii_index < list.size()
                    ? (int) config.initially_selected_mii_index
                    : 0;
            data.index = initialIndex;
            AlertDialog.Builder builder =
                    new AlertDialog.Builder(emulationActivity)
                            .setTitle(config.title.isEmpty()
                                    ? emulationActivity.getString(R.string.mii_selector)
                                    : config.title)
                            .setSingleChoiceItems(list.toArray(new String[]{}), initialIndex,
                                    (dialog, which) -> {
                                        data.index = which;
                                    })
                            .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                                data.return_code = 0;
                                synchronized (finishLock) {
                                    finishLock.notifyAll();
                                }
                            });
            if (config.enable_cancel_button) {
                builder.setNegativeButton(android.R.string.cancel, (dialog, which) -> {
                    data.return_code = 1;
                    synchronized (finishLock) {
                        finishLock.notifyAll();
                    }
                });
            }
            setCancelable(false);
            return builder.create();
        }
    }

    private static MiiSelectorData data;
    private static final Object finishLock = new Object();

    private static void ExecuteImpl(MiiSelectorConfig config) {
        final EmulationActivity emulationActivity = NativeLibrary.sEmulationActivity.get();

        data = new MiiSelectorData(0, 0);

        MiiSelectorDialogFragment fragment = MiiSelectorDialogFragment.newInstance(config);
        fragment.show(emulationActivity.getSupportFragmentManager(), "mii_selector");
    }

    public static MiiSelectorData Execute(MiiSelectorConfig config) {
        NativeLibrary.sEmulationActivity.get().runOnUiThread(() -> ExecuteImpl(config));

        synchronized (finishLock) {
            try {
                finishLock.wait();
            } catch (Exception ignored) {
            }
        }

        return data;
    }
}
