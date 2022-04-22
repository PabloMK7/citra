package org.citra.citra_emu.utils;

import android.graphics.Bitmap;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Request;
import com.squareup.picasso.RequestHandler;

import org.citra.citra_emu.NativeLibrary;

import java.nio.IntBuffer;

public class GameIconRequestHandler extends RequestHandler {
    @Override
    public boolean canHandleRequest(Request data) {
        return "iso".equals(data.uri.getScheme());
    }

    @Override
    public Result load(Request request, int networkPolicy) {
        String url = request.uri.getHost() + request.uri.getPath();
        int[] vector = NativeLibrary.GetIcon(url);
        Bitmap bitmap = Bitmap.createBitmap(48, 48, Bitmap.Config.RGB_565);
        bitmap.copyPixelsFromBuffer(IntBuffer.wrap(vector));
        return new Result(bitmap, Picasso.LoadedFrom.DISK);
    }
}
