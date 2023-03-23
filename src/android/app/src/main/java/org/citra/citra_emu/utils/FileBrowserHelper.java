package org.citra.citra_emu.utils;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.Nullable;
import androidx.documentfile.provider.DocumentFile;

import java.util.ArrayList;
import java.util.List;

public final class FileBrowserHelper {

    @Nullable
    public static String[] getSelectedFiles(Intent result, Context context, List<String> extension) {
        ClipData clipData = result.getClipData();
        List<DocumentFile> files = new ArrayList<>();
        if (clipData == null) {
            files.add(DocumentFile.fromSingleUri(context, result.getData()));
        } else {
            for (int i = 0; i < clipData.getItemCount(); i++) {
                ClipData.Item item = clipData.getItemAt(i);
                Uri uri = item.getUri();
                files.add(DocumentFile.fromSingleUri(context, uri));
            }
        }
        if (!files.isEmpty()) {
            List<String> filePaths = new ArrayList<>();
            for (int i = 0; i < files.size(); i++) {
                DocumentFile file = files.get(i);
                String filename = file.getName();
                int extensionStart = filename.lastIndexOf('.');
                if (extensionStart > 0) {
                    String fileExtension = filename.substring(extensionStart + 1);
                    if (extension.contains(fileExtension)) {
                        filePaths.add(file.getUri().toString());
                    }
                }
            }
            if (filePaths.isEmpty()) {
                return null;
            }
            return filePaths.toArray(new String[0]);
        }

        return null;
    }
}
