// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.features.cheats.model

import androidx.annotation.Keep

@Keep
class CheatEngine(titleId: Long) {
    @Keep
    private val mPointer: Long

    init {
        mPointer = initialize(titleId)
    }

    protected external fun finalize()

    external fun getCheats(): Array<Cheat>

    external fun addCheat(cheat: Cheat?)
    external fun removeCheat(index: Int)
    external fun updateCheat(index: Int, newCheat: Cheat?)
    external fun saveCheatFile()

    companion object {
        @JvmStatic
        private external fun initialize(titleId: Long): Long
    }
}
