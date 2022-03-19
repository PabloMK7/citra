package org.citra.citra_emu.features.settings.ui;

import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Menu;
import android.view.MenuInflater;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentTransaction;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.utils.DirectoryInitialization;
import org.citra.citra_emu.utils.DirectoryStateReceiver;
import org.citra.citra_emu.utils.EmulationMenuSettings;

public final class SettingsActivity extends AppCompatActivity implements SettingsActivityView {
    private static final String ARG_MENU_TAG = "menu_tag";
    private static final String ARG_GAME_ID = "game_id";
    private static final String FRAGMENT_TAG = "settings";
    private SettingsActivityPresenter mPresenter = new SettingsActivityPresenter(this);

    private ProgressDialog dialog;

    public static void launch(Context context, String menuTag, String gameId) {
        Intent settings = new Intent(context, SettingsActivity.class);
        settings.putExtra(ARG_MENU_TAG, menuTag);
        settings.putExtra(ARG_GAME_ID, gameId);
        context.startActivity(settings);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_settings);

        Intent launcher = getIntent();
        String gameID = launcher.getStringExtra(ARG_GAME_ID);
        String menuTag = launcher.getStringExtra(ARG_MENU_TAG);

        mPresenter.onCreate(savedInstanceState, menuTag, gameID);

        // Show "Back" button in the action bar for navigation
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
    }

    @Override
    public boolean onSupportNavigateUp() {
        onBackPressed();

        return true;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu_settings, menu);

        return true;
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        // Critical: If super method is not called, rotations will be busted.
        super.onSaveInstanceState(outState);
        mPresenter.saveState(outState);
    }

    @Override
    protected void onStart() {
        super.onStart();
        mPresenter.onStart();
    }

    /**
     * If this is called, the user has left the settings screen (potentially through the
     * home button) and will expect their changes to be persisted. So we kick off an
     * IntentService which will do so on a background thread.
     */
    @Override
    protected void onStop() {
        super.onStop();

        mPresenter.onStop(isFinishing());

        // Update framebuffer layout when closing the settings
        NativeLibrary.NotifyOrientationChange(EmulationMenuSettings.getLandscapeScreenLayout(),
                getWindowManager().getDefaultDisplay().getRotation());
    }

    @Override
    public void showSettingsFragment(String menuTag, boolean addToStack, String gameID) {
        if (!addToStack && getFragment() != null) {
            return;
        }

        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();

        if (addToStack) {
            if (areSystemAnimationsEnabled()) {
                transaction.setCustomAnimations(
                        R.animator.settings_enter,
                        R.animator.settings_exit,
                        R.animator.settings_pop_enter,
                        R.animator.setttings_pop_exit);
            }

            transaction.addToBackStack(null);
        }
        transaction.replace(R.id.frame_content, SettingsFragment.newInstance(menuTag, gameID), FRAGMENT_TAG);

        transaction.commit();
    }

    private boolean areSystemAnimationsEnabled() {
        float duration = Settings.Global.getFloat(
                getContentResolver(),
                Settings.Global.ANIMATOR_DURATION_SCALE, 1);
        float transition = Settings.Global.getFloat(
                getContentResolver(),
                Settings.Global.TRANSITION_ANIMATION_SCALE, 1);
        return duration != 0 && transition != 0;
    }

    @Override
    public void startDirectoryInitializationService(DirectoryStateReceiver receiver, IntentFilter filter) {
        LocalBroadcastManager.getInstance(this).registerReceiver(
                receiver,
                filter);
        DirectoryInitialization.start(this);
    }

    @Override
    public void stopListeningToDirectoryInitializationService(DirectoryStateReceiver receiver) {
        LocalBroadcastManager.getInstance(this).unregisterReceiver(receiver);
    }

    @Override
    public void showLoading() {
        if (dialog == null) {
            dialog = new ProgressDialog(this);
            dialog.setMessage(getString(R.string.load_settings));
            dialog.setIndeterminate(true);
        }

        dialog.show();
    }

    @Override
    public void hideLoading() {
        dialog.dismiss();
    }

    @Override
    public void showPermissionNeededHint() {
        Toast.makeText(this, R.string.write_permission_needed, Toast.LENGTH_SHORT)
                .show();
    }

    @Override
    public void showExternalStorageNotMountedHint() {
        Toast.makeText(this, R.string.external_storage_not_mounted, Toast.LENGTH_SHORT)
                .show();
    }

    @Override
    public org.citra.citra_emu.features.settings.model.Settings getSettings() {
        return mPresenter.getSettings();
    }

    @Override
    public void setSettings(org.citra.citra_emu.features.settings.model.Settings settings) {
        mPresenter.setSettings(settings);
    }

    @Override
    public void onSettingsFileLoaded(org.citra.citra_emu.features.settings.model.Settings settings) {
        SettingsFragmentView fragment = getFragment();

        if (fragment != null) {
            fragment.onSettingsFileLoaded(settings);
        }
    }

    @Override
    public void onSettingsFileNotFound() {
        SettingsFragmentView fragment = getFragment();

        if (fragment != null) {
            fragment.loadDefaultSettings();
        }
    }

    @Override
    public void showToastMessage(String message, boolean is_long) {
        Toast.makeText(this, message, is_long ? Toast.LENGTH_LONG : Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onSettingChanged() {
        mPresenter.onSettingChanged();
    }

    private SettingsFragment getFragment() {
        return (SettingsFragment) getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG);
    }
}
