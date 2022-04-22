/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.citra.citra_emu.utils;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Environment;
import android.preference.PreferenceManager;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.citra.citra_emu.NativeLibrary;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A service that spawns its own thread in order to copy several binary and shader files
 * from the Citra APK to the external file system.
 */
public final class DirectoryInitialization {
    public static final String BROADCAST_ACTION = "org.citra.citra_emu.BROADCAST";

    public static final String EXTRA_STATE = "directoryState";
    private static volatile DirectoryInitializationState directoryState = null;
    private static String userPath;
    private static AtomicBoolean isCitraDirectoryInitializationRunning = new AtomicBoolean(false);

    public static void start(Context context) {
        // Can take a few seconds to run, so don't block UI thread.
        //noinspection TrivialFunctionalExpressionUsage
        ((Runnable) () -> init(context)).run();
    }

    private static void init(Context context) {
        if (!isCitraDirectoryInitializationRunning.compareAndSet(false, true))
            return;

        if (directoryState != DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED) {
            if (PermissionsHandler.hasWriteAccess(context)) {
                if (setCitraUserDirectory()) {
                    initializeInternalStorage(context);
                    NativeLibrary.CreateConfigFile();
                    directoryState = DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED;
                } else {
                    directoryState = DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE;
                }
            } else {
                directoryState = DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED;
            }
        }

        isCitraDirectoryInitializationRunning.set(false);
        sendBroadcastState(directoryState, context);
    }

    private static void deleteDirectoryRecursively(File file) {
        if (file.isDirectory()) {
            for (File child : file.listFiles())
                deleteDirectoryRecursively(child);
        }
        file.delete();
    }

    public static boolean areCitraDirectoriesReady() {
        return directoryState == DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED;
    }

    public static String getUserDirectory() {
        if (directoryState == null) {
            throw new IllegalStateException("DirectoryInitialization has to run at least once!");
        } else if (isCitraDirectoryInitializationRunning.get()) {
            throw new IllegalStateException(
                    "DirectoryInitialization has to finish running first!");
        }
        return userPath;
    }

    private static native void SetSysDirectory(String path);

    private static boolean setCitraUserDirectory() {
        if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState())) {
            File externalPath = Environment.getExternalStorageDirectory();
            if (externalPath != null) {
                userPath = externalPath.getAbsolutePath() + "/citra-emu";
                Log.debug("[DirectoryInitialization] User Dir: " + userPath);
                // NativeLibrary.SetUserDirectory(userPath);
                return true;
            }

        }

        return false;
    }

    private static void initializeInternalStorage(Context context) {
        File sysDirectory = new File(context.getFilesDir(), "Sys");

        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        String revision = NativeLibrary.GetGitRevision();
        if (!preferences.getString("sysDirectoryVersion", "").equals(revision)) {
            // There is no extracted Sys directory, or there is a Sys directory from another
            // version of Citra that might contain outdated files. Let's (re-)extract Sys.
            deleteDirectoryRecursively(sysDirectory);
            copyAssetFolder("Sys", sysDirectory, true, context);

            SharedPreferences.Editor editor = preferences.edit();
            editor.putString("sysDirectoryVersion", revision);
            editor.apply();
        }

        // Let the native code know where the Sys directory is.
        SetSysDirectory(sysDirectory.getPath());
    }

    private static void sendBroadcastState(DirectoryInitializationState state, Context context) {
        Intent localIntent =
                new Intent(BROADCAST_ACTION)
                        .putExtra(EXTRA_STATE, state);
        LocalBroadcastManager.getInstance(context).sendBroadcast(localIntent);
    }

    private static void copyAsset(String asset, File output, Boolean overwrite, Context context) {
        Log.verbose("[DirectoryInitialization] Copying File " + asset + " to " + output);

        try {
            if (!output.exists() || overwrite) {
                InputStream in = context.getAssets().open(asset);
                OutputStream out = new FileOutputStream(output);
                copyFile(in, out);
                in.close();
                out.close();
            }
        } catch (IOException e) {
            Log.error("[DirectoryInitialization] Failed to copy asset file: " + asset +
                    e.getMessage());
        }
    }

    private static void copyAssetFolder(String assetFolder, File outputFolder, Boolean overwrite,
                                        Context context) {
        Log.verbose("[DirectoryInitialization] Copying Folder " + assetFolder + " to " +
                outputFolder);

        try {
            boolean createdFolder = false;
            for (String file : context.getAssets().list(assetFolder)) {
                if (!createdFolder) {
                    outputFolder.mkdir();
                    createdFolder = true;
                }
                copyAssetFolder(assetFolder + File.separator + file, new File(outputFolder, file),
                        overwrite, context);
                copyAsset(assetFolder + File.separator + file, new File(outputFolder, file), overwrite,
                        context);
            }
        } catch (IOException e) {
            Log.error("[DirectoryInitialization] Failed to copy asset folder: " + assetFolder +
                    e.getMessage());
        }
    }

    private static void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;

        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }

    public enum DirectoryInitializationState {
        CITRA_DIRECTORIES_INITIALIZED,
        EXTERNAL_STORAGE_PERMISSION_NEEDED,
        CANT_FIND_EXTERNAL_STORAGE
    }
}
