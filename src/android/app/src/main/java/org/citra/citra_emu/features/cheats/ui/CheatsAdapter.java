package org.citra.citra_emu.features.cheats.ui;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.cheats.model.Cheat;
import org.citra.citra_emu.features.cheats.model.CheatsViewModel;

public class CheatsAdapter extends RecyclerView.Adapter<CheatViewHolder> {
    private final CheatsActivity mActivity;
    private final CheatsViewModel mViewModel;

    public CheatsAdapter(CheatsActivity activity, CheatsViewModel viewModel) {
        mActivity = activity;
        mViewModel = viewModel;

        mViewModel.getCheatAddedEvent().observe(activity, (position) -> {
            if (position != null) {
                notifyItemInserted(position);
            }
        });

        mViewModel.getCheatUpdatedEvent().observe(activity, (position) -> {
            if (position != null) {
                notifyItemChanged(position);
            }
        });

        mViewModel.getCheatDeletedEvent().observe(activity, (position) -> {
            if (position != null) {
                notifyItemRemoved(position);
            }
        });
    }

    @NonNull
    @Override
    public CheatViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        View cheatView = inflater.inflate(R.layout.list_item_cheat, parent, false);
        addViewListeners(cheatView);
        return new CheatViewHolder(cheatView);
    }

    @Override
    public void onBindViewHolder(@NonNull CheatViewHolder holder, int position) {
        holder.bind(mActivity, getItemAt(position), position);
    }

    @Override
    public int getItemCount() {
        return mViewModel.getCheats().length;
    }

    private void addViewListeners(View view) {
        // On a portrait phone screen (or other narrow screen), only one of the two panes are shown
        // at the same time. If the user is navigating using a d-pad and moves focus to an element
        // in the currently hidden pane, we need to manually show that pane.
        CheatsActivity.setOnFocusChangeListenerRecursively(view,
                (v, hasFocus) -> mActivity.onListViewFocusChange(hasFocus));
    }

    private Cheat getItemAt(int position) {
        return mViewModel.getCheats()[position];
    }
}
