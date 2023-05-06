package org.citra.citra_emu.model;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.IOException;

public class GameInfo {
    @Keep
    private final long mPointer;

    @Keep
    public GameInfo(String path) throws IOException {
        mPointer = initialize(path);
        if (mPointer == 0L) {
            throw new IOException();
        }
    }

    private static native long initialize(String path);

    @Override
    protected native void finalize();

    @NonNull
    public native String getTitle();

    @NonNull
    public native String getRegions();

    @NonNull
    public native String getCompany();

    @Nullable
    public native int[] getIcon();
}
