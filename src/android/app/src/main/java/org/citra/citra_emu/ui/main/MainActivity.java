package org.citra.citra_emu.ui.main;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.FrameLayout;
import android.widget.Toast;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.core.splashscreen.SplashScreen;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import java.util.Collections;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

import com.google.android.material.appbar.AppBarLayout;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;
import org.citra.citra_emu.contracts.OpenFileResultContract;
import org.citra.citra_emu.features.settings.ui.SettingsActivity;
import org.citra.citra_emu.model.GameProvider;
import org.citra.citra_emu.ui.platform.PlatformGamesFragment;
import org.citra.citra_emu.utils.AddDirectoryHelper;
import org.citra.citra_emu.utils.BillingManager;
import org.citra.citra_emu.utils.CitraDirectoryHelper;
import org.citra.citra_emu.utils.DirectoryInitialization;
import org.citra.citra_emu.utils.FileBrowserHelper;
import org.citra.citra_emu.utils.InsetsHelper;
import org.citra.citra_emu.utils.PermissionsHandler;
import org.citra.citra_emu.utils.PicassoUtils;
import org.citra.citra_emu.utils.StartupHandler;
import org.citra.citra_emu.utils.ThemeUtil;

/**
 * The main Activity of the Lollipop style UI. Manages several PlatformGamesFragments, which
 * individually display a grid of available games for each Fragment, in a tabbed layout.
 */
public final class MainActivity extends AppCompatActivity implements MainView {
    private Toolbar mToolbar;
    private int mFrameLayoutId;
    private PlatformGamesFragment mPlatformGamesFragment;

    private MainPresenter mPresenter = new MainPresenter(this);

    // Singleton to manage user billing state
    private static BillingManager mBillingManager;

    private static MenuItem mPremiumButton;

    private final CitraDirectoryHelper citraDirectoryHelper = new CitraDirectoryHelper(this, () -> {
        // If mPlatformGamesFragment is null means game directory have not been set yet.
        if (mPlatformGamesFragment == null) {
            mPlatformGamesFragment = new PlatformGamesFragment();
            getSupportFragmentManager()
                .beginTransaction()
                .add(mFrameLayoutId, mPlatformGamesFragment)
                .commit();
            showGameInstallDialog();
        }
    });

    private final ActivityResultLauncher<Uri> mOpenCitraDirectory =
        registerForActivityResult(new ActivityResultContracts.OpenDocumentTree(), result -> {
            if (result == null)
                return;
            citraDirectoryHelper.showCitraDirectoryDialog(result);
        });

    private final ActivityResultLauncher<Uri> mOpenGameListLauncher =
        registerForActivityResult(new ActivityResultContracts.OpenDocumentTree(), result -> {
            if (result == null)
                return;
            int takeFlags =
                (Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION);
            getContentResolver().takePersistableUriPermission(result, takeFlags);
            // When a new directory is picked, we currently will reset the existing games
            // database. This effectively means that only one game directory is supported.
            // TODO(bunnei): Consider fixing this in the future, or removing code for this.
            getContentResolver().insert(GameProvider.URI_RESET, null);
            // Add the new directory
            mPresenter.onDirectorySelected(result.toString());
        });

    private final ActivityResultLauncher<Boolean> mOpenFileLauncher =
        registerForActivityResult(new OpenFileResultContract(), result -> {
            if (result == null)
                return;
            String[] selectedFiles = FileBrowserHelper.getSelectedFiles(
                result, getApplicationContext(), Collections.singletonList("cia"));
            if (selectedFiles == null) {
                Toast
                    .makeText(getApplicationContext(), R.string.cia_file_not_found,
                              Toast.LENGTH_LONG)
                    .show();
                return;
            }
            NativeLibrary.InstallCIAS(selectedFiles);
            mPresenter.refreshGameList();
        });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        SplashScreen splashScreen = SplashScreen.installSplashScreen(this);
        splashScreen.setKeepOnScreenCondition(
            ()
                -> (PermissionsHandler.hasWriteAccess(this) &&
                    !DirectoryInitialization.areCitraDirectoriesReady()));

        ThemeUtil.applyTheme(this);

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);

        findViews();

        setSupportActionBar(mToolbar);

        mFrameLayoutId = R.id.games_platform_frame;
        mPresenter.onCreate();

        if (savedInstanceState == null) {
            StartupHandler.HandleInit(this, mOpenCitraDirectory);
            if (PermissionsHandler.hasWriteAccess(this)) {
                mPlatformGamesFragment = new PlatformGamesFragment();
                getSupportFragmentManager().beginTransaction().add(mFrameLayoutId, mPlatformGamesFragment)
                        .commit();
            }
        } else {
            mPlatformGamesFragment = (PlatformGamesFragment) getSupportFragmentManager().getFragment(savedInstanceState, "mPlatformGamesFragment");
        }
        PicassoUtils.init();

        // Setup billing manager, so we can globally query for Premium status
        mBillingManager = new BillingManager(this);

        // Dismiss previous notifications (should not happen unless a crash occurred)
        EmulationActivity.tryDismissRunningNotification(this);

        setInsets();
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        if (PermissionsHandler.hasWriteAccess(this)) {
            if (getSupportFragmentManager() == null) {
                return;
            }
            if (outState == null) {
                return;
            }
            getSupportFragmentManager().putFragment(outState, "mPlatformGamesFragment", mPlatformGamesFragment);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mPresenter.addDirIfNeeded(new AddDirectoryHelper(this));
    }

    // TODO: Replace with a ButterKnife injection.
    private void findViews() {
        mToolbar = findViewById(R.id.toolbar_main);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.menu_game_grid, menu);
        mPremiumButton = menu.findItem(R.id.button_premium);

        if (mBillingManager.isPremiumCached()) {
            // User had premium in a previous session, hide upsell option
            setPremiumButtonVisible(false);
        }

        return true;
    }

    static public void setPremiumButtonVisible(boolean isVisible) {
        if (mPremiumButton != null) {
            mPremiumButton.setVisible(isVisible);
        }
    }

    /**
     * MainView
     */

    @Override
    public void setVersionString(String version) {
        mToolbar.setSubtitle(version);
    }

    @Override
    public void refresh() {
        getContentResolver().insert(GameProvider.URI_REFRESH, null);
        refreshFragment();
    }

    @Override
    public void launchSettingsActivity(String menuTag) {
        if (PermissionsHandler.hasWriteAccess(this)) {
            SettingsActivity.launch(this, menuTag, "");
        } else {
            PermissionsHandler.checkWritePermission(this, mOpenCitraDirectory);
        }
    }

    @Override
    public void launchFileListActivity(int request) {
        if (PermissionsHandler.hasWriteAccess(this)) {
            switch (request) {
                case MainPresenter.REQUEST_SELECT_CITRA_DIRECTORY:
                mOpenCitraDirectory.launch(null);
                break;
                case MainPresenter.REQUEST_ADD_DIRECTORY:
                mOpenGameListLauncher.launch(null);
                break;
                case MainPresenter.REQUEST_INSTALL_CIA:
                mOpenFileLauncher.launch(true);
                break;
            }
        } else {
            PermissionsHandler.checkWritePermission(this, mOpenCitraDirectory);
        }
    }

    /**
     * Called by the framework whenever any actionbar/toolbar icon is clicked.
     *
     * @param item The icon that was clicked on.
     * @return True if the event was handled, false to bubble it up to the OS.
     */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return mPresenter.handleOptionSelection(item.getItemId());
    }

    private void refreshFragment() {
        if (mPlatformGamesFragment != null) {
            mPlatformGamesFragment.refresh();
        }
    }

    private void showGameInstallDialog() {
        new MaterialAlertDialogBuilder(this)
            .setIcon(R.mipmap.ic_launcher)
            .setTitle(R.string.app_name)
            .setMessage(R.string.app_game_install_description)
            .setCancelable(false)
            .setNegativeButton(android.R.string.cancel, null)
            .setPositiveButton(android.R.string.ok,
                               (d, v) -> mOpenGameListLauncher.launch(null))
            .show();
    }

    @Override
    protected void onDestroy() {
        EmulationActivity.tryDismissRunningNotification(this);
        super.onDestroy();
    }

    /**
     * @return true if Premium subscription is currently active
     */
    public static boolean isPremiumActive() {
        return mBillingManager.isPremiumActive();
    }

    /**
     * Invokes the billing flow for Premium
     *
     * @param callback Optional callback, called once, on completion of billing
     */
    public static void invokePremiumBilling(Runnable callback) {
        mBillingManager.invokePremiumBilling(callback);
    }

    private void setInsets() {
        AppBarLayout appBar = findViewById(R.id.appbar);
        FrameLayout frame = findViewById(R.id.games_platform_frame);
        ViewCompat.setOnApplyWindowInsetsListener(frame, (v, windowInsets) -> {
            Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
            InsetsHelper.insetAppBar(insets, appBar);
            frame.setPadding(insets.left, 0, insets.right, 0);
            return windowInsets;
        });
    }
}
