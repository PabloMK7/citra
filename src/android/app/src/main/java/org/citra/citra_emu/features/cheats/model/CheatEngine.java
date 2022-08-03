package org.citra.citra_emu.features.cheats.model;

public class CheatEngine {
    public static native Cheat[] getCheats();

    public static native void addCheat(Cheat cheat);

    public static native void removeCheat(int index);

    public static native void updateCheat(int index, Cheat newCheat);

    public static native void saveCheatFile();
}
