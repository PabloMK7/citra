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
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.lifecycle.ViewModelProvider;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import com.google.android.material.appbar.AppBarLayout;
import com.google.android.material.appbar.MaterialToolbar;

import org.citra.citra_emu.R;
import org.citra.citra_emu.features.cheats.model.Cheat;
import org.citra.citra_emu.features.cheats.model.CheatsViewModel;
import org.citra.citra_emu.ui.TwoPaneOnBackPressedCallback;
import org.citra.citra_emu.utils.InsetsHelper;
import org.citra.citra_emu.utils.ThemeUtil;

import java.util.List;

public class CheatsActivity extends AppCompatActivity
        implements SlidingPaneLayout.PanelSlideListener {
    private static String ARG_TITLE_ID = "title_id";

    private CheatsViewModel mViewModel;

    private SlidingPaneLayout mSlidingPaneLayout;
    private View mCheatList;
    private View mCheatDetails;

    private View mCheatListLastFocus;
    private View mCheatDetailsLastFocus;

    public static void launch(Context context, long titleId) {
        Intent intent = new Intent(context, CheatsActivity.class);
        intent.putExtra(ARG_TITLE_ID, titleId);
        context.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ThemeUtil.applyTheme(this);

        super.onCreate(savedInstanceState);

        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);

        long titleId = getIntent().getLongExtra(ARG_TITLE_ID, -1);

        mViewModel = new ViewModelProvider(this).get(CheatsViewModel.class);
        mViewModel.initialize(titleId);

        setContentView(R.layout.activity_cheats);

        mSlidingPaneLayout = findViewById(R.id.sliding_pane_layout);
        mCheatList = findViewById(R.id.cheat_list_container);
        mCheatDetails = findViewById(R.id.cheat_details_container);

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
        MaterialToolbar toolbar = findViewById(R.id.toolbar_cheats);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        setInsets();
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

    public static void setOnFocusChangeListenerRecursively(@NonNull View view, View.OnFocusChangeListener listener) {
        view.setOnFocusChangeListener(listener);

        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                View child = viewGroup.getChildAt(i);
                setOnFocusChangeListenerRecursively(child, listener);
            }
        }
    }

    private void setInsets() {
        AppBarLayout appBarLayout = findViewById(R.id.appbar_cheats);
        ViewCompat.setOnApplyWindowInsetsListener(mSlidingPaneLayout, (v, windowInsets) -> {
            Insets barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
            Insets keyboardInsets = windowInsets.getInsets(WindowInsetsCompat.Type.ime());

            InsetsHelper.insetAppBar(barInsets, appBarLayout);
            mSlidingPaneLayout.setPadding(barInsets.left, 0, barInsets.right, 0);

            // Set keyboard insets if the system supports smooth keyboard animations
            ViewGroup.MarginLayoutParams mlpDetails =
                    (ViewGroup.MarginLayoutParams) mCheatDetails.getLayoutParams();
            if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.R) {
                if (keyboardInsets.bottom > 0) {
                    mlpDetails.bottomMargin = keyboardInsets.bottom;
                } else {
                    mlpDetails.bottomMargin = barInsets.bottom;
                }
            } else {
                if (mlpDetails.bottomMargin == 0) {
                    mlpDetails.bottomMargin = barInsets.bottom;
                }
            }
            mCheatDetails.setLayoutParams(mlpDetails);

            return windowInsets;
        });

        // Update the layout for every frame that the keyboard animates in
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            ViewCompat.setWindowInsetsAnimationCallback(mCheatDetails,
                    new WindowInsetsAnimationCompat.Callback(
                            WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_STOP) {
                        int keyboardInsets = 0;
                        int barInsets = 0;

                        @NonNull
                        @Override
                        public WindowInsetsCompat onProgress(@NonNull WindowInsetsCompat insets,
                                                             @NonNull List<WindowInsetsAnimationCompat> runningAnimations) {
                            ViewGroup.MarginLayoutParams mlpDetails =
                                    (ViewGroup.MarginLayoutParams) mCheatDetails.getLayoutParams();
                            keyboardInsets = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom;
                            barInsets = insets.getInsets(WindowInsetsCompat.Type.systemBars()).bottom;
                            mlpDetails.bottomMargin = Math.max(keyboardInsets, barInsets);
                            mCheatDetails.setLayoutParams(mlpDetails);
                            return insets;
                        }
                    });
        }
    }
}
