// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra_emu.citra;

import android.app.Application;

public class CitraApplication extends Application {
    static {
        System.loadLibrary("citra-android");
    }
}
