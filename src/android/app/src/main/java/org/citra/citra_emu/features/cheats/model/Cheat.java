package org.citra.citra_emu.features.cheats.model;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class Cheat {
    @Keep
    private final long mPointer;

    private Runnable mEnabledChangedCallback = null;

    @Keep
    private Cheat(long pointer) {
        mPointer = pointer;
    }

    @Override
    protected native void finalize();

    @NonNull
    public native String getName();

    @NonNull
    public native String getNotes();

    @NonNull
    public native String getCode();

    public native boolean getEnabled();

    public void setEnabled(boolean enabled) {
        setEnabledImpl(enabled);
        onEnabledChanged();
    }

    private native void setEnabledImpl(boolean enabled);

    public void setEnabledChangedCallback(@Nullable Runnable callback) {
        mEnabledChangedCallback = callback;
    }

    private void onEnabledChanged() {
        if (mEnabledChangedCallback != null) {
            mEnabledChangedCallback.run();
        }
    }

    /**
     * If the code is valid, returns 0. Otherwise, returns the 1-based index
     * for the line containing the error.
     */
    public static native int isValidGatewayCode(@NonNull String code);

    public static native Cheat createGatewayCode(@NonNull String name, @NonNull String notes,
                                                 @NonNull String code);
}
