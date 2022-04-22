package org.citra.citra_emu.utils;

import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;

import com.squareup.picasso.Transformation;

public class PicassoRoundedCornersTransformation implements Transformation {
    @Override
    public Bitmap transform(Bitmap icon) {
        final int width = icon.getWidth();
        final int height = icon.getHeight();
        final Rect rect = new Rect(0, 0, width, height);
        final int size = Math.min(width, height);
        final int x = (width - size) / 2;
        final int y = (height - size) / 2;

        Bitmap squaredBitmap = Bitmap.createBitmap(icon, x, y, size, size);
        if (squaredBitmap != icon) {
            icon.recycle();
        }

        Bitmap output = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(output);
        BitmapShader shader = new BitmapShader(squaredBitmap, BitmapShader.TileMode.CLAMP, BitmapShader.TileMode.CLAMP);
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setShader(shader);

        canvas.drawRoundRect(new RectF(rect), 10, 10, paint);

        squaredBitmap.recycle();

        return output;
    }

    @Override
    public String key() {
        return "circle";
    }
}