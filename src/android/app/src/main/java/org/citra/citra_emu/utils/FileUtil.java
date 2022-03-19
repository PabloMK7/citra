package org.citra.citra_emu.utils;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

public class FileUtil {
    public static byte[] getBytesFromFile(File file) throws IOException {
        final long length = file.length();

        // You cannot create an array using a long type.
        if (length > Integer.MAX_VALUE) {
            // File is too large
            throw new IOException("File is too large!");
        }

        byte[] bytes = new byte[(int) length];

        int offset = 0;
        int numRead;

        try (InputStream is = new FileInputStream(file)) {
            while (offset < bytes.length
                    && (numRead = is.read(bytes, offset, bytes.length - offset)) >= 0) {
                offset += numRead;
            }
        }

        // Ensure all the bytes have been read in
        if (offset < bytes.length) {
            throw new IOException("Could not completely read file " + file.getName());
        }

        return bytes;
    }
}
