package org.citra.citra_emu.utils;

import android.content.Intent;
import android.net.Uri;
import androidx.fragment.app.FragmentActivity;
import java.util.concurrent.Executors;
import org.citra.citra_emu.dialogs.CitraDirectoryDialog;
import org.citra.citra_emu.dialogs.CopyDirProgressDialog;

/**
 * Citra directory initialization ui flow controller.
 */
public class CitraDirectoryHelper {
    public interface Listener {
        void onDirectoryInitialized();
    }

    private final FragmentActivity mFragmentActivity;
    private final Listener mListener;

    public CitraDirectoryHelper(FragmentActivity mFragmentActivity, Listener mListener) {
        this.mFragmentActivity = mFragmentActivity;
        this.mListener = mListener;
    }

    public void showCitraDirectoryDialog(Uri result) {
        CitraDirectoryDialog citraDirectoryDialog = CitraDirectoryDialog.newInstance(
            result.toString(), ((moveData, path) -> {
                Uri previous = PermissionsHandler.getCitraDirectory();
                // Do noting if user select the previous path.
                if (path.equals(previous)) {
                    return;
                }
                int takeFlags = (Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
                                 Intent.FLAG_GRANT_READ_URI_PERMISSION);
                mFragmentActivity.getContentResolver().takePersistableUriPermission(path,
                                                                                    takeFlags);
                if (!moveData || previous == null) {
                    initializeCitraDirectory(path);
                    mListener.onDirectoryInitialized();
                    return;
                }

                // If user check move data, show copy progress dialog.
                showCopyDialog(previous, path);
            }));
        citraDirectoryDialog.show(mFragmentActivity.getSupportFragmentManager(),
                                  CitraDirectoryDialog.TAG);
    }

    private void showCopyDialog(Uri previous, Uri path) {
        CopyDirProgressDialog copyDirProgressDialog = new CopyDirProgressDialog();
        copyDirProgressDialog.showNow(mFragmentActivity.getSupportFragmentManager(),
                                      CopyDirProgressDialog.TAG);

        // Run copy dir in background
        Executors.newSingleThreadExecutor().execute(() -> {
            FileUtil.copyDir(
                mFragmentActivity, previous.toString(), path.toString(),
                new FileUtil.CopyDirListener() {
                    @Override
                    public void onSearchProgress(String directoryName) {
                        copyDirProgressDialog.onUpdateSearchProgress(directoryName);
                    }

                    @Override
                    public void onCopyProgress(String filename, int progress, int max) {
                        copyDirProgressDialog.onUpdateCopyProgress(filename, progress, max);
                    }

                    @Override
                    public void onComplete() {
                        initializeCitraDirectory(path);
                        copyDirProgressDialog.dismissAllowingStateLoss();
                        mListener.onDirectoryInitialized();
                    }
                });
        });
    }

    private void initializeCitraDirectory(Uri path) {
        if (!PermissionsHandler.setCitraDirectory(path.toString()))
            return;
        DirectoryInitialization.resetCitraDirectoryState();
        DirectoryInitialization.start(mFragmentActivity);
    }
}
