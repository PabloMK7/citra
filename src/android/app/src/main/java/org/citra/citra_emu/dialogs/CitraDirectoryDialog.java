package org.citra.citra_emu.dialogs;

import android.app.Dialog;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentActivity;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import java.util.Objects;
import org.citra.citra_emu.R;
import org.citra.citra_emu.utils.FileUtil;
import org.citra.citra_emu.utils.PermissionsHandler;

public class CitraDirectoryDialog extends DialogFragment {
    public static final String TAG = "citra_directory_dialog_fragment";

    private static final String MOVE_DATE_ENABLE = "IS_MODE_DATA_ENABLE";

    TextView pathView;

    TextView spaceView;

    CheckBox checkBox;

    AlertDialog dialog;

    Listener listener;

    public interface Listener {
        void onPressPositiveButton(boolean moveData, Uri path);
    }

    public static CitraDirectoryDialog newInstance(String path, Listener listener) {
        CitraDirectoryDialog frag = new CitraDirectoryDialog();
        frag.listener = listener;
        Bundle args = new Bundle();
        args.putString("path", path);
        frag.setArguments(args);
        return frag;
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        final FragmentActivity activity = requireActivity();
        final Uri path = Uri.parse(Objects.requireNonNull(requireArguments().getString("path")));
        SharedPreferences mPreferences = PreferenceManager.getDefaultSharedPreferences(activity);
        String freeSpaceText =
            getResources().getString(R.string.free_space, FileUtil.getFreeSpace(activity, path));

        LayoutInflater inflater = getLayoutInflater();
        View view = inflater.inflate(R.layout.dialog_citra_directory, null);

        checkBox = view.findViewById(R.id.checkBox);
        pathView = view.findViewById(R.id.path);
        spaceView = view.findViewById(R.id.space);

        checkBox.setChecked(mPreferences.getBoolean(MOVE_DATE_ENABLE, true));
        if (!PermissionsHandler.hasWriteAccess(activity)) {
            checkBox.setVisibility(View.GONE);
        }
        checkBox.setOnCheckedChangeListener(
            (v, isChecked)
                    // record move data selection with SharedPreferences
                -> mPreferences.edit().putBoolean(MOVE_DATE_ENABLE, checkBox.isChecked()).apply());

        pathView.setText(path.getPath());
        spaceView.setText(freeSpaceText);

        setCancelable(false);

        dialog = new MaterialAlertDialogBuilder(activity)
                     .setView(view)
                     .setIcon(R.mipmap.ic_launcher)
                     .setTitle(R.string.app_name)
                     .setPositiveButton(
                         android.R.string.ok,
                         (d, v) -> listener.onPressPositiveButton(checkBox.isChecked(), path))
                     .setNegativeButton(android.R.string.cancel, null)
                     .create();
        return dialog;
    }
}
