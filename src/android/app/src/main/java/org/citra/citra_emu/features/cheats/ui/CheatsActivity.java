package org.citra.citra_emu.features.cheats.ui;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.ViewCompat;
import androidx.lifecycle.ViewModelProvider;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.cheats.model.Cheat;
import org.citra.citra_emu.features.cheats.model.CheatsViewModel;
import org.citra.citra_emu.ui.TwoPaneOnBackPressedCallback;

public class CheatsActivity extends AppCompatActivity
        implements SlidingPaneLayout.PanelSlideListener {
    private CheatsViewModel mViewModel;

    private SlidingPaneLayout mSlidingPaneLayout;
    private View mCheatList;
    private View mCheatDetails;

    private View mCheatListLastFocus;
    private View mCheatDetailsLastFocus;

    public static void launch(Context context) {
        Intent intent = new Intent(context, CheatsActivity.class);
        context.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mViewModel = new ViewModelProvider(this).get(CheatsViewModel.class);
        mViewModel.load();

        setContentView(R.layout.activity_cheats);

        mSlidingPaneLayout = findViewById(R.id.sliding_pane_layout);
        mCheatList = findViewById(R.id.cheat_list);
        mCheatDetails = findViewById(R.id.cheat_details);

        mCheatListLastFocus = mCheatList;
        mCheatDetailsLastFocus = mCheatDetails;

        mSlidingPaneLayout.addPanelSlideListener(this);

        getOnBackPressedDispatcher().addCallback(this,
                new TwoPaneOnBackPressedCallback(mSlidingPaneLayout));

        mViewModel.getSelectedCheat().observe(this, this::onSelectedCheatChanged);
        mViewModel.getIsEditing().observe(this, this::onIsEditingChanged);
        onSelectedCheatChanged(mViewModel.getSelectedCheat().getValue());

        mViewModel.getOpenDetailsViewEvent().observe(this, this::openDetailsView);

        // Show "Up" button in the action bar for navigation
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu_settings, menu);

        return true;
    }

    @Override
    protected void onStop() {
        super.onStop();

        mViewModel.saveIfNeeded();
    }

    @Override
    public void onPanelSlide(@NonNull View panel, float slideOffset) {
    }

    @Override
    public void onPanelOpened(@NonNull View panel) {
        boolean rtl = ViewCompat.getLayoutDirection(panel) == ViewCompat.LAYOUT_DIRECTION_RTL;
        mCheatDetailsLastFocus.requestFocus(rtl ? View.FOCUS_LEFT : View.FOCUS_RIGHT);
    }

    @Override
    public void onPanelClosed(@NonNull View panel) {
        boolean rtl = ViewCompat.getLayoutDirection(panel) == ViewCompat.LAYOUT_DIRECTION_RTL;
        mCheatListLastFocus.requestFocus(rtl ? View.FOCUS_RIGHT : View.FOCUS_LEFT);
    }

    private void onIsEditingChanged(boolean isEditing) {
        if (isEditing) {
            mSlidingPaneLayout.setLockMode(SlidingPaneLayout.LOCK_MODE_UNLOCKED);
        }
    }

    private void onSelectedCheatChanged(Cheat selectedCheat) {
        boolean cheatSelected = selectedCheat != null || mViewModel.getIsEditing().getValue();

        if (!cheatSelected && mSlidingPaneLayout.isOpen()) {
            mSlidingPaneLayout.close();
        }

        mSlidingPaneLayout.setLockMode(cheatSelected ?
                SlidingPaneLayout.LOCK_MODE_UNLOCKED : SlidingPaneLayout.LOCK_MODE_LOCKED_CLOSED);
    }

    public void onListViewFocusChange(boolean hasFocus) {
        if (hasFocus) {
            mCheatListLastFocus = mCheatList.findFocus();
            if (mCheatListLastFocus == null)
                throw new NullPointerException();

            mSlidingPaneLayout.close();
        }
    }

    public void onDetailsViewFocusChange(boolean hasFocus) {
        if (hasFocus) {
            mCheatDetailsLastFocus = mCheatDetails.findFocus();
            if (mCheatDetailsLastFocus == null)
                throw new NullPointerException();

            mSlidingPaneLayout.open();
        }
    }

    @Override
    public boolean onSupportNavigateUp() {
        onBackPressed();
        return true;
    }

    private void openDetailsView(boolean open) {
        if (open) {
            mSlidingPaneLayout.open();
        }
    }

    public static void setOnFocusChangeListenerRecursively(@NonNull View view,
                                                           View.OnFocusChangeListener listener) {
        view.setOnFocusChangeListener(listener);

        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                View child = viewGroup.getChildAt(i);
                setOnFocusChangeListenerRecursively(child, listener);
            }
        }
    }
}
