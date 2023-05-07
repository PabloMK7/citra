package org.citra.citra_emu.features.cheats.model;

import androidx.annotation.Keep;

public class CheatEngine {
    @Keep
    private final long mPointer;

    @Keep
    public CheatEngine(long titleId) {
        mPointer = initialize(titleId);
    }

    private static native long initialize(long titleId);

    @Override
    protected native void finalize();

    public native Cheat[] getCheats();

    public native void addCheat(Cheat cheat);

    public native void removeCheat(int index);

    public native void updateCheat(int index, Cheat newCheat);

    public native void saveCheatFile();
}
