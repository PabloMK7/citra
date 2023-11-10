package org.citra.citra_emu.utils;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.core.app.NotificationCompat;
import androidx.work.ForegroundInfo;
import androidx.work.Worker;
import androidx.work.WorkerParameters;

import org.citra.citra_emu.NativeLibrary.InstallStatus;
import org.citra.citra_emu.R;

public class CiaInstallWorker extends Worker {
    private final Context mContext = getApplicationContext();

    private final NotificationManager mNotificationManager =
            mContext.getSystemService(NotificationManager.class);

    static final String GROUP_KEY_CIA_INSTALL_STATUS = "org.citra.citra_emu.CIA_INSTALL_STATUS";

    private final NotificationCompat.Builder mInstallProgressBuilder = new NotificationCompat.Builder(
            mContext, mContext.getString(R.string.cia_install_notification_channel_id))
            .setContentTitle(mContext.getString(R.string.install_cia_title))
            .setContentIntent(PendingIntent.getBroadcast(mContext, 0,
                    new Intent("CitraDoNothing"), PendingIntent.FLAG_IMMUTABLE))
            .setSmallIcon(R.drawable.ic_stat_notification_logo);

    private final NotificationCompat.Builder mInstallStatusBuilder = new NotificationCompat.Builder(
            mContext, mContext.getString(R.string.cia_install_notification_channel_id))
            .setContentTitle(mContext.getString(R.string.install_cia_title))
            .setSmallIcon(R.drawable.ic_stat_notification_logo)
            .setGroup(GROUP_KEY_CIA_INSTALL_STATUS);

    private final Notification mSummaryNotification =
            new NotificationCompat.Builder(mContext, mContext.getString(R.string.cia_install_notification_channel_id))
                    .setContentTitle(mContext.getString(R.string.install_cia_title))
                    .setSmallIcon(R.drawable.ic_stat_notification_logo)
                    .setGroup(GROUP_KEY_CIA_INSTALL_STATUS)
                    .setGroupSummary(true)
                    .build();

    private static long mLastNotifiedTime = 0;

    private static final int SUMMARY_NOTIFICATION_ID = 0xC1A0000;
    private static final int PROGRESS_NOTIFICATION_ID = SUMMARY_NOTIFICATION_ID + 1;
    private static int mStatusNotificationId = SUMMARY_NOTIFICATION_ID + 2;

    public CiaInstallWorker(
            @NonNull Context context,
            @NonNull WorkerParameters params) {
        super(context, params);
    }

    private void notifyInstallStatus(String filename, InstallStatus status) {
        switch(status){
            case Success:
                mInstallStatusBuilder.setContentTitle(
                        mContext.getString(R.string.cia_install_notification_success_title));
                mInstallStatusBuilder.setContentText(
                        mContext.getString(R.string.cia_install_success, filename));
                break;
            case ErrorAborted:
                mInstallStatusBuilder.setContentTitle(
                        mContext.getString(R.string.cia_install_notification_error_title));
                mInstallStatusBuilder.setStyle(new NotificationCompat.BigTextStyle()
                                .bigText(mContext.getString(
                                         R.string.cia_install_error_aborted, filename)));
                break;
            case ErrorInvalid:
                mInstallStatusBuilder.setContentTitle(
                        mContext.getString(R.string.cia_install_notification_error_title));
                mInstallStatusBuilder.setContentText(
                        mContext.getString(R.string.cia_install_error_invalid, filename));
                break;
            case ErrorEncrypted:
                mInstallStatusBuilder.setContentTitle(
                        mContext.getString(R.string.cia_install_notification_error_title));
                mInstallStatusBuilder.setStyle(new NotificationCompat.BigTextStyle()
                        .bigText(mContext.getString(
                                 R.string.cia_install_error_encrypted, filename)));
                break;
            case ErrorFailedToOpenFile:
                // TODO:
            case ErrorFileNotFound:
                // shouldn't happen
            default:
                mInstallStatusBuilder.setContentTitle(
                        mContext.getString(R.string.cia_install_notification_error_title));
                mInstallStatusBuilder.setStyle(new NotificationCompat.BigTextStyle()
                        .bigText(mContext.getString(R.string.cia_install_error_unknown, filename)));
                break;
        }
        // Even if newer versions of Android don't show the group summary text that you design,
        // you always need to manually set a summary to enable grouped notifications.
        mNotificationManager.notify(SUMMARY_NOTIFICATION_ID, mSummaryNotification);
        mNotificationManager.notify(mStatusNotificationId++, mInstallStatusBuilder.build());
    }
    @NonNull
    @Override
    public Result doWork() {
        String[] selectedFiles = getInputData().getStringArray("CIA_FILES");
        assert selectedFiles != null;
        final CharSequence toastText = mContext.getResources().getQuantityString(R.plurals.cia_install_toast,
                selectedFiles.length, selectedFiles.length);

        getApplicationContext().getMainExecutor().execute(() -> Toast.makeText(mContext, toastText,
                Toast.LENGTH_LONG).show());

        // Issue the initial notification with zero progress
        mInstallProgressBuilder.setOngoing(true);
        setProgressCallback(100, 0);

        int i = 0;
        for (String file : selectedFiles) {
            String filename = FileUtil.getFilename(Uri.parse(file));
            mInstallProgressBuilder.setContentText(mContext.getString(
                    R.string.cia_install_notification_installing, filename, ++i, selectedFiles.length));
            InstallStatus res = installCIA(file);
            notifyInstallStatus(filename, res);
        }
        mNotificationManager.cancel(PROGRESS_NOTIFICATION_ID);

        return Result.success();
    }
    public void setProgressCallback(int max, int progress) {
        long currentTime = System.currentTimeMillis();
        // Android applies a rate limit when updating a notification.
        // If you post updates to a single notification too frequently,
        // such as many in less than one second, the system might drop updates.
        // TODO: consider moving to C++ side
        if (currentTime - mLastNotifiedTime < 500 /* ms */){
            return;
        }
        mLastNotifiedTime = currentTime;
        mInstallProgressBuilder.setProgress(max, progress, false);
        mNotificationManager.notify(PROGRESS_NOTIFICATION_ID, mInstallProgressBuilder.build());
    }

    @NonNull
    @Override
    public ForegroundInfo getForegroundInfo() {
        return new ForegroundInfo(PROGRESS_NOTIFICATION_ID, mInstallProgressBuilder.build());
    }

    private native InstallStatus installCIA(String path);
}
