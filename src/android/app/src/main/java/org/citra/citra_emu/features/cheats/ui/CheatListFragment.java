package org.citra.citra_emu.features.cheats.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.floatingactionbutton.FloatingActionButton;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.cheats.model.CheatsViewModel;
import org.citra.citra_emu.ui.DividerItemDecoration;

public class CheatListFragment extends Fragment {
    private RecyclerView mRecyclerView;
    private FloatingActionButton mFab;

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_cheat_list, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        mRecyclerView = view.findViewById(R.id.cheat_list);
        mFab = view.findViewById(R.id.fab);

        CheatsActivity activity = (CheatsActivity) requireActivity();
        CheatsViewModel viewModel = new ViewModelProvider(activity).get(CheatsViewModel.class);

        mRecyclerView.setAdapter(new CheatsAdapter(activity, viewModel));
        mRecyclerView.setLayoutManager(new LinearLayoutManager(activity));
        mRecyclerView.addItemDecoration(new DividerItemDecoration(activity, null));

        mFab.setOnClickListener(v -> {
            viewModel.startAddingCheat();
            viewModel.openDetailsView();
        });

        setInsets();
    }

    private void setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(mRecyclerView, (v, windowInsets) -> {
            Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(0, 0, 0, insets.bottom + getResources().getDimensionPixelSize(R.dimen.spacing_fab_list));

            ViewGroup.MarginLayoutParams mlpFab =
                    (ViewGroup.MarginLayoutParams) mFab.getLayoutParams();
            int fabPadding = getResources().getDimensionPixelSize(R.dimen.spacing_large);
            mlpFab.leftMargin = insets.left + fabPadding;
            mlpFab.bottomMargin = insets.bottom + fabPadding;
            mlpFab.rightMargin = insets.right + fabPadding;
            mFab.setLayoutParams(mlpFab);

            return windowInsets;
        });
    }
}
