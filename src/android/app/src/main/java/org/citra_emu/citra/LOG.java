package org.citra_emu.citra;

public class LOG {

    private interface LOG_LEVEL {
        int TRACE = 0, DEBUG = 1, INFO = 2, WARNING = 3, ERROR = 4, CRITICAL = 5;
    }

    public static void TRACE(String msg, Object... args) {
        LOG(LOG_LEVEL.TRACE, msg, args);
    }

    public static void DEBUG(String msg, Object... args) {
        LOG(LOG_LEVEL.DEBUG, msg, args);
    }

    public static void INFO(String msg, Object... args) {
        LOG(LOG_LEVEL.INFO, msg, args);
    }

    public static void WARNING(String msg, Object... args) {
        LOG(LOG_LEVEL.WARNING, msg, args);
    }

    public static void ERROR(String msg, Object... args) {
        LOG(LOG_LEVEL.ERROR, msg, args);
    }

    public static void CRITICAL(String msg, Object... args) {
        LOG(LOG_LEVEL.CRITICAL, msg, args);
    }

    private static void LOG(int level, String msg, Object... args) {
        StackTraceElement trace = Thread.currentThread().getStackTrace()[4];
        logEntry(level, trace.getFileName(), trace.getLineNumber(), trace.getMethodName(),
                 String.format(msg, args));
    }

    private static native void logEntry(int level, String file_name, int line_number,
                                        String function, String message);
}
