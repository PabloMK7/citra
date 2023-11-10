package org.citra.citra_emu.utils;

import android.graphics.Bitmap;
import android.net.Uri;

import com.squareup.picasso.Picasso;

import java.io.IOException;

import androidx.annotation.Nullable;

public class PicassoUtils {
    // Blocking call. Load image from file and crop/resize it to fit in width x height.
    @Nullable
    public static Bitmap LoadBitmapFromFile(String uri, int width, int height) {
        try {
            return Picasso.get()
                    .load(Uri.parse(uri))
                    .config(Bitmap.Config.ARGB_8888)
                    .centerCrop()
                    .resize(width, height)
                    .get();
        } catch (IOException e) {
            return null;
        }
    }
}
