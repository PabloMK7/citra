// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.util.Log
import org.citra.citra_emu.BuildConfig

/**
 * Contains methods that call through to [android.util.Log], but
 * with the same TAG automatically provided. Also no-ops VERBOSE and DEBUG log
 * levels in release builds.
 */
object Log {
    // Tracks whether we should share the old log or the current log
    var gameLaunched = false
    private const val TAG = "Citra Frontend"

    fun verbose(message: String?) {
        if (BuildConfig.DEBUG) {
            Log.v(TAG, message!!)
        }
    }

    fun debug(message: String?) {
        if (BuildConfig.DEBUG) {
            Log.d(TAG, message!!)
        }
    }

    fun info(message: String?) = Log.i(TAG, message!!)

    fun warning(message: String?) = Log.w(TAG, message!!)

    fun error(message: String?) = Log.e(TAG, message!!)
}
