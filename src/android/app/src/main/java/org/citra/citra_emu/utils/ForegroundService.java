/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.citra.citra_emu.utils;

import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;

/**
 * A service that shows a permanent notification in the background to avoid the app getting
 * cleared from memory by the system.
 */
public class ForegroundService extends Service {
    private static final int EMULATION_RUNNING_NOTIFICATION = 0x1000;

    private void showRunningNotification() {
        // Intent is used to resume emulation if the notification is clicked
        PendingIntent contentIntent = PendingIntent.getActivity(this, 0,
                new Intent(this, EmulationActivity.class), PendingIntent.FLAG_UPDATE_CURRENT);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, getString(R.string.app_notification_channel_id))
                .setSmallIcon(R.drawable.ic_stat_notification_logo)
                .setContentTitle(getString(R.string.app_name))
                .setContentText(getString(R.string.app_notification_running))
                .setPriority(NotificationCompat.PRIORITY_LOW)
                .setOngoing(true)
                .setVibrate(null)
                .setSound(null)
                .setContentIntent(contentIntent);
        startForeground(EMULATION_RUNNING_NOTIFICATION, builder.build());
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        showRunningNotification();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        NotificationManagerCompat.from(this).cancel(EMULATION_RUNNING_NOTIFICATION);
    }
}
