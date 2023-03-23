package org.citra.citra_emu.dialogs;

import android.app.Dialog;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentActivity;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import org.citra.citra_emu.R;

public class CopyDirProgressDialog extends DialogFragment {
    public static final String TAG = "copy_dir_progress_dialog";
    ProgressBar progressBar;

    TextView progressText;

    AlertDialog dialog;

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        final FragmentActivity activity = requireActivity();

        LayoutInflater inflater = getLayoutInflater();
        View view = inflater.inflate(R.layout.dialog_progress_bar, null);

        progressBar = view.findViewById(R.id.progress_bar);
        progressText = view.findViewById(R.id.progress_text);
        progressText.setText("");

        setCancelable(false);

        dialog = new MaterialAlertDialogBuilder(activity)
                     .setView(view)
                     .setIcon(R.mipmap.ic_launcher)
                     .setTitle(R.string.move_data)
                     .setMessage("")
                     .create();
        return dialog;
    }

    public void onUpdateSearchProgress(String msg) {
        requireActivity().runOnUiThread(() -> {
            dialog.setMessage(getResources().getString(R.string.searching_direcotry, msg));
        });
    }

    public void onUpdateCopyProgress(String msg, int progress, int max) {
        requireActivity().runOnUiThread(() -> {
            progressBar.setProgress(progress);
            progressBar.setMax(max);
            progressText.setText(String.format("%d/%d", progress, max));
            dialog.setMessage(getResources().getString(R.string.copy_file_name, msg));
        });
    }
}
