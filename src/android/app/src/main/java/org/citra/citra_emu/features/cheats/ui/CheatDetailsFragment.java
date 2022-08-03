package org.citra.citra_emu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.cheats.model.Cheat;
import org.citra.citra_emu.features.cheats.model.CheatsViewModel;

public class CheatDetailsFragment extends Fragment {
    private View mRoot;
    private ScrollView mScrollView;
    private TextView mLabelName;
    private EditText mEditName;
    private EditText mEditNotes;
    private EditText mEditCode;
    private Button mButtonDelete;
    private Button mButtonEdit;
    private Button mButtonCancel;
    private Button mButtonOk;

    private CheatsViewModel mViewModel;

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_cheat_details, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        mRoot = view.findViewById(R.id.root);
        mScrollView = view.findViewById(R.id.scroll_view);
        mLabelName = view.findViewById(R.id.label_name);
        mEditName = view.findViewById(R.id.edit_name);
        mEditNotes = view.findViewById(R.id.edit_notes);
        mEditCode = view.findViewById(R.id.edit_code);
        mButtonDelete = view.findViewById(R.id.button_delete);
        mButtonEdit = view.findViewById(R.id.button_edit);
        mButtonCancel = view.findViewById(R.id.button_cancel);
        mButtonOk = view.findViewById(R.id.button_ok);

        CheatsActivity activity = (CheatsActivity) requireActivity();
        mViewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);

        mViewModel.getSelectedCheat().observe(getViewLifecycleOwner(),
                this::onSelectedCheatUpdated);
        mViewModel.getIsEditing().observe(getViewLifecycleOwner(), this::onIsEditingUpdated);

        mButtonDelete.setOnClickListener(this::onDeleteClicked);
        mButtonEdit.setOnClickListener(this::onEditClicked);
        mButtonCancel.setOnClickListener(this::onCancelClicked);
        mButtonOk.setOnClickListener(this::onOkClicked);

        // On a portrait phone screen (or other narrow screen), only one of the two panes are shown
        // at the same time. If the user is navigating using a d-pad and moves focus to an element
        // in the currently hidden pane, we need to manually show that pane.
        CheatsActivity.setOnFocusChangeListenerRecursively(view,
                (v, hasFocus) -> activity.onDetailsViewFocusChange(hasFocus));
    }

    private void clearEditErrors() {
        mEditName.setError(null);
        mEditCode.setError(null);
    }

    private void onDeleteClicked(View view) {
        String name = mEditName.getText().toString();

        AlertDialog.Builder builder = new AlertDialog.Builder(requireContext());
        builder.setMessage(getString(R.string.cheats_delete_confirmation, name));
        builder.setPositiveButton(android.R.string.yes,
                (dialog, i) -> mViewModel.deleteSelectedCheat());
        builder.setNegativeButton(android.R.string.no, null);
        builder.show();
    }

    private void onEditClicked(View view) {
        mViewModel.setIsEditing(true);
        mButtonOk.requestFocus();
    }

    private void onCancelClicked(View view) {
        mViewModel.setIsEditing(false);
        onSelectedCheatUpdated(mViewModel.getSelectedCheat().getValue());
        mButtonDelete.requestFocus();
    }

    private void onOkClicked(View view) {
        clearEditErrors();

        String name = mEditName.getText().toString();
        String notes = mEditNotes.getText().toString();
        String code = mEditCode.getText().toString();

        if (name.isEmpty()) {
            mEditName.setError(getString(R.string.cheats_error_no_name));
            mScrollView.smoothScrollTo(0, mLabelName.getTop());
            return;
        } else if (code.isEmpty()) {
            mEditCode.setError(getString(R.string.cheats_error_no_code_lines));
            mScrollView.smoothScrollTo(0, mEditCode.getBottom());
            return;
        }

        int validityResult = Cheat.isValidGatewayCode(code);

        if (validityResult != 0) {
            mEditCode.setError(getString(R.string.cheats_error_on_line, validityResult));
            mScrollView.smoothScrollTo(0, mEditCode.getBottom());
            return;
        }

        Cheat newCheat = Cheat.createGatewayCode(name, notes, code);

        if (mViewModel.getIsAdding().getValue()) {
            mViewModel.finishAddingCheat(newCheat);
        } else {
            mViewModel.updateSelectedCheat(newCheat);
        }

        mButtonEdit.requestFocus();
    }

    private void onSelectedCheatUpdated(@Nullable Cheat cheat) {
        clearEditErrors();

        boolean isEditing = mViewModel.getIsEditing().getValue();

        mRoot.setVisibility(isEditing || cheat != null ? View.VISIBLE : View.GONE);

        // If the fragment was recreated while editing a cheat, it's vital that we
        // don't repopulate the fields, otherwise the user's changes will be lost
        if (!isEditing) {
            if (cheat == null) {
                mEditName.setText("");
                mEditNotes.setText("");
                mEditCode.setText("");
            } else {
                mEditName.setText(cheat.getName());
                mEditNotes.setText(cheat.getNotes());
                mEditCode.setText(cheat.getCode());
            }
        }
    }

    private void onIsEditingUpdated(boolean isEditing) {
        if (isEditing) {
            mRoot.setVisibility(View.VISIBLE);
        }

        mEditName.setEnabled(isEditing);
        mEditNotes.setEnabled(isEditing);
        mEditCode.setEnabled(isEditing);

        mButtonDelete.setVisibility(isEditing ? View.GONE : View.VISIBLE);
        mButtonEdit.setVisibility(isEditing ? View.GONE : View.VISIBLE);
        mButtonCancel.setVisibility(isEditing ? View.VISIBLE : View.GONE);
        mButtonOk.setVisibility(isEditing ? View.VISIBLE : View.GONE);
    }
}
