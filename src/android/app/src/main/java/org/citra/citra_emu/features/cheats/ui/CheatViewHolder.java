package org.citra.citra_emu.features.cheats.ui;

import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.RecyclerView;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.cheats.model.Cheat;
import org.citra.citra_emu.features.cheats.model.CheatsViewModel;

public class CheatViewHolder extends RecyclerView.ViewHolder
        implements View.OnClickListener, CompoundButton.OnCheckedChangeListener {
    private final View mRoot;
    private final TextView mName;
    private final CheckBox mCheckbox;

    private CheatsViewModel mViewModel;
    private Cheat mCheat;
    private int mPosition;

    public CheatViewHolder(@NonNull View itemView) {
        super(itemView);

        mRoot = itemView.findViewById(R.id.root);
        mName = itemView.findViewById(R.id.text_name);
        mCheckbox = itemView.findViewById(R.id.checkbox);
    }

    public void bind(CheatsActivity activity, Cheat cheat, int position) {
        mCheckbox.setOnCheckedChangeListener(null);

        mViewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);
        mCheat = cheat;
        mPosition = position;

        mName.setText(mCheat.getName());
        mCheckbox.setChecked(mCheat.getEnabled());

        mRoot.setOnClickListener(this);
        mCheckbox.setOnCheckedChangeListener(this);
    }

    public void onClick(View root) {
        mViewModel.setSelectedCheat(mCheat, mPosition);
        mViewModel.openDetailsView();
    }

    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        mCheat.setEnabled(isChecked);
    }
}
