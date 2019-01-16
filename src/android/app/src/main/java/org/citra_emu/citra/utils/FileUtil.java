// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra_emu.citra.utils;

import android.os.Environment;

import java.io.File;

public class FileUtil {
    public static File getUserPath() {
        File storage = Environment.getExternalStorageDirectory();
        File userPath = new File(storage, "citra");
        if (!userPath.isDirectory())
            userPath.mkdir();
        return userPath;
    }
}
