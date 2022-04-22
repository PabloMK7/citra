// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.camera;

import android.content.Intent;
import android.graphics.Bitmap;
import android.provider.MediaStore;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;
import org.citra.citra_emu.utils.PicassoUtils;

import androidx.annotation.Nullable;

// Used in native code.
public final class StillImageCameraHelper {
    public static final int REQUEST_CAMERA_FILE_PICKER = 1;
    private static final Object filePickerLock = new Object();
    private static @Nullable
    String filePickerPath;

    // Opens file picker for camera.
    public static @Nullable
    String OpenFilePicker() {
        final EmulationActivity emulationActivity = NativeLibrary.sEmulationActivity.get();

        // At this point, we are assuming that we already have permissions as they are
        // needed to launch a game
        emulationActivity.runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_PICK);
            intent.setDataAndType(MediaStore.Images.Media.INTERNAL_CONTENT_URI, "image/*");
            emulationActivity.startActivityForResult(
                    Intent.createChooser(intent,
                            emulationActivity.getString(R.string.camera_select_image)),
                    REQUEST_CAMERA_FILE_PICKER);
        });

        synchronized (filePickerLock) {
            try {
                filePickerLock.wait();
            } catch (InterruptedException ignored) {
            }
        }

        return filePickerPath;
    }

    // Called from EmulationActivity.
    public static void OnFilePickerResult(Intent result) {
        filePickerPath = result == null ? null : result.getDataString();

        synchronized (filePickerLock) {
            filePickerLock.notifyAll();
        }
    }

    // Blocking call. Load image from file and crop/resize it to fit in width x height.
    @Nullable
    public static Bitmap LoadImageFromFile(String uri, int width, int height) {
        return PicassoUtils.LoadBitmapFromFile(uri, width, height);
    }
}
