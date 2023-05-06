package org.citra.citra_emu.utils;

import android.graphics.Bitmap;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Request;
import com.squareup.picasso.RequestHandler;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.model.GameInfo;

import java.io.IOException;
import java.nio.IntBuffer;

public class GameIconRequestHandler extends RequestHandler {
    @Override
    public boolean canHandleRequest(Request data) {
        return "content".equals(data.uri.getScheme()) || data.uri.getScheme() == null;
    }

    @Override
    public Result load(Request request, int networkPolicy) {
        int[] vector;
        try {
            String url = request.uri.toString();
            vector = new GameInfo(url).getIcon();
        } catch (IOException e) {
            vector = null;
        }

        Bitmap bitmap = Bitmap.createBitmap(48, 48, Bitmap.Config.RGB_565);
        bitmap.copyPixelsFromBuffer(IntBuffer.wrap(vector));
        return new Result(bitmap, Picasso.LoadedFrom.DISK);
    }
}
