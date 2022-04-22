// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.applets;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.InputFilter;
import android.text.Spanned;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;
import org.citra.citra_emu.utils.Log;

import java.util.Objects;

public final class SoftwareKeyboard {
    /// Corresponds to Frontend::ButtonConfig
    private interface ButtonConfig {
        int Single = 0; /// Ok button
        int Dual = 1;   /// Cancel | Ok buttons
        int Triple = 2; /// Cancel | I Forgot | Ok buttons
        int None = 3;   /// No button (returned by swkbdInputText in special cases)
    }

    /// Corresponds to Frontend::ValidationError
    public enum ValidationError {
        None,
        // Button Selection
        ButtonOutOfRange,
        // Configured Filters
        MaxDigitsExceeded,
        AtSignNotAllowed,
        PercentNotAllowed,
        BackslashNotAllowed,
        ProfanityNotAllowed,
        CallbackFailed,
        // Allowed Input Type
        FixedLengthRequired,
        MaxLengthExceeded,
        BlankInputNotAllowed,
        EmptyInputNotAllowed,
    }

    public static class KeyboardConfig implements java.io.Serializable {
        public int button_config;
        public int max_text_length;
        public boolean multiline_mode; /// True if the keyboard accepts multiple lines of input
        public String hint_text;       /// Displayed in the field as a hint before
        @Nullable
        public String[] button_text; /// Contains the button text that the caller provides
    }

    /// Corresponds to Frontend::KeyboardData
    public static class KeyboardData {
        public int button;
        public String text;

        private KeyboardData(int button, String text) {
            this.button = button;
            this.text = text;
        }
    }

    private static class Filter implements InputFilter {
        @Override
        public CharSequence filter(CharSequence source, int start, int end, Spanned dest,
                                   int dstart, int dend) {
            String text = new StringBuilder(dest)
                    .replace(dstart, dend, source.subSequence(start, end).toString())
                    .toString();
            if (ValidateFilters(text) == ValidationError.None) {
                return null; // Accept replacement
            }
            return dest.subSequence(dstart, dend); // Request the subsequence to be unchanged
        }
    }

    public static class KeyboardDialogFragment extends DialogFragment {
        static KeyboardDialogFragment newInstance(KeyboardConfig config) {
            KeyboardDialogFragment frag = new KeyboardDialogFragment();
            Bundle args = new Bundle();
            args.putSerializable("config", config);
            frag.setArguments(args);
            return frag;
        }

        @NonNull
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final Activity emulationActivity = getActivity();
            assert emulationActivity != null;

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            params.leftMargin = params.rightMargin =
                    CitraApplication.getAppContext().getResources().getDimensionPixelSize(
                            R.dimen.dialog_margin);

            KeyboardConfig config = Objects.requireNonNull(
                    (KeyboardConfig) Objects.requireNonNull(getArguments()).getSerializable("config"));

            // Set up the input
            EditText editText = new EditText(CitraApplication.getAppContext());
            editText.setHint(config.hint_text);
            editText.setSingleLine(!config.multiline_mode);
            editText.setLayoutParams(params);
            editText.setFilters(new InputFilter[]{
                    new Filter(), new InputFilter.LengthFilter(config.max_text_length)});

            FrameLayout container = new FrameLayout(emulationActivity);
            container.addView(editText);

            AlertDialog.Builder builder = new AlertDialog.Builder(emulationActivity)
                    .setTitle(R.string.software_keyboard)
                    .setView(container);
            setCancelable(false);

            switch (config.button_config) {
                case ButtonConfig.Triple: {
                    final String text = config.button_text[1].isEmpty()
                            ? emulationActivity.getString(R.string.i_forgot)
                            : config.button_text[1];
                    builder.setNeutralButton(text, null);
                }
                // fallthrough
                case ButtonConfig.Dual: {
                    final String text = config.button_text[0].isEmpty()
                            ? emulationActivity.getString(android.R.string.cancel)
                            : config.button_text[0];
                    builder.setNegativeButton(text, null);
                }
                // fallthrough
                case ButtonConfig.Single: {
                    final String text = config.button_text[2].isEmpty()
                            ? emulationActivity.getString(android.R.string.ok)
                            : config.button_text[2];
                    builder.setPositiveButton(text, null);
                    break;
                }
            }

            final AlertDialog dialog = builder.create();
            dialog.create();
            if (dialog.getButton(DialogInterface.BUTTON_POSITIVE) != null) {
                dialog.getButton(DialogInterface.BUTTON_POSITIVE).setOnClickListener((view) -> {
                    data.button = config.button_config;
                    data.text = editText.getText().toString();
                    final ValidationError error = ValidateInput(data.text);
                    if (error != ValidationError.None) {
                        HandleValidationError(config, error);
                        return;
                    }

                    dialog.dismiss();

                    synchronized (finishLock) {
                        finishLock.notifyAll();
                    }
                });
            }
            if (dialog.getButton(DialogInterface.BUTTON_NEUTRAL) != null) {
                dialog.getButton(DialogInterface.BUTTON_NEUTRAL).setOnClickListener((view) -> {
                    data.button = 1;
                    dialog.dismiss();
                    synchronized (finishLock) {
                        finishLock.notifyAll();
                    }
                });
            }
            if (dialog.getButton(DialogInterface.BUTTON_NEGATIVE) != null) {
                dialog.getButton(DialogInterface.BUTTON_NEGATIVE).setOnClickListener((view) -> {
                    data.button = 0;
                    dialog.dismiss();
                    synchronized (finishLock) {
                        finishLock.notifyAll();
                    }
                });
            }

            return dialog;
        }
    }

    private static KeyboardData data;
    private static final Object finishLock = new Object();

    private static void ExecuteImpl(KeyboardConfig config) {
        final EmulationActivity emulationActivity = NativeLibrary.sEmulationActivity.get();

        data = new KeyboardData(0, "");

        KeyboardDialogFragment fragment = KeyboardDialogFragment.newInstance(config);
        fragment.show(emulationActivity.getSupportFragmentManager(), "keyboard");
    }

    private static void HandleValidationError(KeyboardConfig config, ValidationError error) {
        final EmulationActivity emulationActivity = NativeLibrary.sEmulationActivity.get();
        String message = "";
        switch (error) {
            case FixedLengthRequired:
                message =
                        emulationActivity.getString(R.string.fixed_length_required, config.max_text_length);
                break;
            case MaxLengthExceeded:
                message =
                        emulationActivity.getString(R.string.max_length_exceeded, config.max_text_length);
                break;
            case BlankInputNotAllowed:
                message = emulationActivity.getString(R.string.blank_input_not_allowed);
                break;
            case EmptyInputNotAllowed:
                message = emulationActivity.getString(R.string.empty_input_not_allowed);
                break;
        }

        new AlertDialog.Builder(emulationActivity)
                .setTitle(R.string.software_keyboard)
                .setMessage(message)
                .setPositiveButton(android.R.string.ok, null)
                .show();
    }

    public static KeyboardData Execute(KeyboardConfig config) {
        if (config.button_config == ButtonConfig.None) {
            Log.error("Unexpected button config None");
            return new KeyboardData(0, "");
        }

        NativeLibrary.sEmulationActivity.get().runOnUiThread(() -> ExecuteImpl(config));

        synchronized (finishLock) {
            try {
                finishLock.wait();
            } catch (Exception ignored) {
            }
        }

        return data;
    }

    public static void ShowError(String error) {
        NativeLibrary.displayAlertMsg(
                CitraApplication.getAppContext().getResources().getString(R.string.software_keyboard),
                error, false);
    }

    private static native ValidationError ValidateFilters(String text);

    private static native ValidationError ValidateInput(String text);
}
