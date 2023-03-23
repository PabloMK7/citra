package org.citra.citra_emu.utils;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.system.Os;
import android.system.StructStatVfs;
import android.util.Pair;
import androidx.annotation.Nullable;
import androidx.documentfile.provider.DocumentFile;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URLDecoder;
import java.util.ArrayList;
import java.util.List;
import org.citra.citra_emu.model.CheapDocument;

public class FileUtil {
    static final String PATH_TREE = "tree";
    static final String DECODE_METHOD = "UTF-8";
    static final String APPLICATION_OCTET_STREAM = "application/octet-stream";
    static final String TEXT_PLAIN = "text/plain";

    public interface CopyDirListener {
        void onSearchProgress(String directoryName);
        void onCopyProgress(String filename, int progress, int max);

        void onComplete();
    }

    /**
     * Create a file from directory with filename.
     *
     * @param context   Application context
     * @param directory parent path for file.
     * @param filename  file display name.
     * @return boolean
     */
    @Nullable
    public static DocumentFile createFile(Context context, String directory, String filename) {
        try {
            Uri directoryUri = Uri.parse(directory);
            DocumentFile parent;
            parent = DocumentFile.fromTreeUri(context, directoryUri);
            if (parent == null) return null;
            filename = URLDecoder.decode(filename, DECODE_METHOD);
            int extensionPosition = filename.lastIndexOf('.');
            String extension = "";
            if (extensionPosition > 0) {
                extension = filename.substring(extensionPosition);
            }
            String mimeType = APPLICATION_OCTET_STREAM;
            if (extension.equals(".txt")) {
                mimeType = TEXT_PLAIN;
            }
            DocumentFile isExist = parent.findFile(filename);
            if (isExist != null) return isExist;
            return parent.createFile(mimeType, filename);
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot create file, error: " + e.getMessage());
        }
        return null;
    }

    /**
     * Create a directory from directory with filename.
     *
     * @param context       Application context
     * @param directory     parent path for directory.
     * @param directoryName directory display name.
     * @return boolean
     */
    @Nullable
    public static DocumentFile createDir(Context context, String directory, String directoryName) {
        try {
            Uri directoryUri = Uri.parse(directory);
            DocumentFile parent;
            parent = DocumentFile.fromTreeUri(context, directoryUri);
            if (parent == null) return null;
            directoryName = URLDecoder.decode(directoryName, DECODE_METHOD);
            DocumentFile isExist = parent.findFile(directoryName);
            if (isExist != null) return isExist;
            return parent.createDirectory(directoryName);
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot create file, error: " + e.getMessage());
        }
        return null;
    }

    /**
     * Open content uri and return file descriptor to JNI.
     *
     * @param context  Application context
     * @param path     Native content uri path
     * @param openmode will be one of "r", "r", "rw", "wa", "rwa"
     * @return file descriptor
     */
    public static int openContentUri(Context context, String path, String openmode) {
        try (ParcelFileDescriptor parcelFileDescriptor =
                 context.getContentResolver().openFileDescriptor(Uri.parse(path), openmode)) {
            if (parcelFileDescriptor == null) {
                Log.error("[FileUtil]: Cannot get the file descriptor from uri: " + path);
                return -1;
            }
            return parcelFileDescriptor.detachFd();
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot open content uri, error: " + e.getMessage());
        }
        return -1;
    }

    /**
     * Reference:  https://stackoverflow.com/questions/42186820/documentfile-is-very-slow
     * This function will be faster than DocumentFile.listFiles
     *
     * @param context Application context
     * @param uri     Directory uri.
     * @return CheapDocument lists.
     */
    public static CheapDocument[] listFiles(Context context, Uri uri) {
        final ContentResolver resolver = context.getContentResolver();
        final String[] columns = new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
        };
        Cursor c = null;
        final List<CheapDocument> results = new ArrayList<>();
        try {
            String docId;
            if (isRootTreeUri(uri)) {
                docId = DocumentsContract.getTreeDocumentId(uri);
            } else {
                docId = DocumentsContract.getDocumentId(uri);
            }
            final Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(uri, docId);
            c = resolver.query(childrenUri, columns, null, null, null);
            while (c.moveToNext()) {
                final String documentId = c.getString(0);
                final String documentName = c.getString(1);
                final String documentMimeType = c.getString(2);
                final Uri documentUri = DocumentsContract.buildDocumentUriUsingTree(uri, documentId);
                CheapDocument document = new CheapDocument(documentName, documentMimeType, documentUri);
                results.add(document);
            }
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot list file error: " + e.getMessage());
        } finally {
            closeQuietly(c);
        }
        return results.toArray(new CheapDocument[0]);
    }

    /**
     * Check whether given path exists.
     *
     * @param path Native content uri path
     * @return bool
     */
    public static boolean Exists(Context context, String path) {
        Cursor c = null;
        try {
            Uri mUri = Uri.parse(path);
            final String[] columns = new String[] {DocumentsContract.Document.COLUMN_DOCUMENT_ID};
            c = context.getContentResolver().query(mUri, columns, null, null, null);
            return c.getCount() > 0;
        } catch (Exception e) {
            Log.info("[FileUtil] Cannot find file from given path, error: " + e.getMessage());
        } finally {
            closeQuietly(c);
        }
        return false;
    }

    /**
     * Check whether given path is a directory
     *
     * @param path content uri path
     * @return bool
     */
    public static boolean isDirectory(Context context, String path) {
        final ContentResolver resolver = context.getContentResolver();
        final String[] columns = new String[] {DocumentsContract.Document.COLUMN_MIME_TYPE};
        boolean isDirectory = false;
        Cursor c = null;
        try {
            Uri mUri = Uri.parse(path);
            c = resolver.query(mUri, columns, null, null, null);
            c.moveToNext();
            final String mimeType = c.getString(0);
            isDirectory = mimeType.equals(DocumentsContract.Document.MIME_TYPE_DIR);
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot list files, error: " + e.getMessage());
        } finally {
            closeQuietly(c);
        }
        return isDirectory;
    }

    /**
     * Get file display name from given path
     *
     * @param path content uri path
     * @return String display name
     */
    public static String getFilename(Context context, String path) {
        final ContentResolver resolver = context.getContentResolver();
        final String[] columns = new String[] {DocumentsContract.Document.COLUMN_DISPLAY_NAME};
        String filename = "";
        Cursor c = null;
        try {
            Uri mUri = Uri.parse(path);
            c = resolver.query(mUri, columns, null, null, null);
            c.moveToNext();
            filename = c.getString(0);
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot get file size, error: " + e.getMessage());
        } finally {
            closeQuietly(c);
        }
        return filename;
    }

    public static String[] getFilesName(Context context, String path) {
        Uri uri = Uri.parse(path);
        List<String> files = new ArrayList<>();
        for (CheapDocument file : FileUtil.listFiles(context, uri)) {
            files.add(file.getFilename());
        }
        return files.toArray(new String[0]);
    }

    /**
     * Get file size from given path.
     *
     * @param path content uri path
     * @return long file size
     */
    public static long getFileSize(Context context, String path) {
        final ContentResolver resolver = context.getContentResolver();
        final String[] columns = new String[] {DocumentsContract.Document.COLUMN_SIZE};
        long size = 0;
        Cursor c = null;
        try {
            Uri mUri = Uri.parse(path);
            c = resolver.query(mUri, columns, null, null, null);
            c.moveToNext();
            size = c.getLong(0);
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot get file size, error: " + e.getMessage());
        } finally {
            closeQuietly(c);
        }
        return size;
    }

    public static boolean copyFile(Context context, String sourcePath, String destinationParentPath, String destinationFilename) {
        try {
            Uri sourceUri = Uri.parse(sourcePath);
            Uri destinationUri = Uri.parse(destinationParentPath);
            DocumentFile destinationParent = DocumentFile.fromTreeUri(context, destinationUri);
            if (destinationParent == null) return false;
            String filename = URLDecoder.decode(destinationFilename, "UTF-8");
            DocumentFile destination = destinationParent.findFile(filename);
            if (destination == null) {
                destination = destinationParent.createFile("application/octet-stream", filename);
            }
            if (destination == null) return false;
            InputStream input = context.getContentResolver().openInputStream(sourceUri);
            OutputStream output = context.getContentResolver().openOutputStream(destination.getUri());
            byte[] buffer = new byte[1024];
            int len;
            while ((len = input.read(buffer)) != -1) {
                output.write(buffer, 0, len);
            }
            input.close();
            output.flush();
            output.close();
            return true;
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot copy file, error: " + e.getMessage());
        }
        return false;
    }

    public static void copyDir(Context context, String sourcePath, String destinationPath,
                               CopyDirListener listener) {
        try {
            Uri sourceUri = Uri.parse(sourcePath);
            Uri destinationUri = Uri.parse(destinationPath);
            final List<Pair<CheapDocument, DocumentFile>> files = new ArrayList<>();
            final List<Pair<Uri, Uri>> dirs = new ArrayList<>();
            dirs.add(new Pair<>(sourceUri, destinationUri));
            // Searching all files which need to be copied and struct the directory in destination.
            while (!dirs.isEmpty()) {
                DocumentFile fromDir = DocumentFile.fromTreeUri(context, dirs.get(0).first);
                DocumentFile toDir = DocumentFile.fromTreeUri(context, dirs.get(0).second);
                if (fromDir == null || toDir == null)
                    continue;
                Uri fromUri = fromDir.getUri();
                if (listener != null) {
                    listener.onSearchProgress(fromUri.getPath());
                }
                CheapDocument[] documents = FileUtil.listFiles(context, fromUri);
                for (CheapDocument document : documents) {
                    String filename = document.getFilename();
                    if (document.isDirectory()) {
                        DocumentFile target = toDir.findFile(filename);
                        if (target == null || !target.exists()) {
                            target = toDir.createDirectory(filename);
                        }
                        if (target == null)
                            continue;
                        dirs.add(new Pair<>(document.getUri(), target.getUri()));
                    } else {
                        DocumentFile target = toDir.findFile(filename);
                        if (target == null || !target.exists()) {
                            target =
                                toDir.createFile(document.getMimeType(), document.getFilename());
                        }
                        if (target == null)
                            continue;
                        files.add(new Pair<>(document, target));
                    }
                }

                dirs.remove(0);
            }

            int total = files.size();
            int progress = 0;
            for (Pair<CheapDocument, DocumentFile> file : files) {
                DocumentFile to = file.second;
                Uri toUri = to.getUri();
                String filename = getFilenameWithExtensions(toUri);
                String toPath = toUri.getPath();
                DocumentFile toParent = to.getParentFile();
                if (toParent == null)
                    continue;
                FileUtil.copyFile(context, file.first.getUri().toString(),
                                  toParent.getUri().toString(), filename);
                progress++;
                if (listener != null) {
                    listener.onCopyProgress(toPath, progress, total);
                }
            }
            if (listener != null) {
                listener.onComplete();
            }
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot copy directory, error: " + e.getMessage());
        }
    }

    public static boolean renameFile(Context context, String path, String destinationFilename) {
        try {
            Uri uri = Uri.parse(path);
            DocumentsContract.renameDocument(context.getContentResolver(), uri, destinationFilename);
            return true;
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot rename file, error: " + e.getMessage());
        }
        return false;
    }

    public static boolean deleteDocument(Context context, String path) {
        try {
            Uri uri = Uri.parse(path);
            DocumentsContract.deleteDocument(context.getContentResolver(), uri);
            return true;
        } catch (Exception e) {
            Log.error("[FileUtil]: Cannot delete document, error: " + e.getMessage());
        }
        return false;
    }

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
            while (offset < bytes.length &&
                   (numRead = is.read(bytes, offset, bytes.length - offset)) >= 0) {
                offset += numRead;
            }
        }

        // Ensure all the bytes have been read in
        if (offset < bytes.length) {
            throw new IOException("Could not completely read file " + file.getName());
        }

        return bytes;
    }

    public static boolean isRootTreeUri(Uri uri) {
        final List<String> paths = uri.getPathSegments();
        return paths.size() == 2 && PATH_TREE.equals(paths.get(0));
    }

    public static boolean isNativePath(String path) {
        try {
            return path.charAt(0) == '/';
        } catch (StringIndexOutOfBoundsException e) {
            Log.error("[FileUtil] Cannot determine the string is native path or not.");
        }
        return false;
    }

    public static String getFilenameWithExtensions(Uri uri) {
        final String path = uri.getPath();
        final int index = path.lastIndexOf('/');
        return path.substring(index + 1);
    }

    public static double getFreeSpace(Context context, Uri uri) {
        try {
            Uri docTreeUri = DocumentsContract.buildDocumentUriUsingTree(
                uri, DocumentsContract.getTreeDocumentId(uri));
            ParcelFileDescriptor pfd =
                context.getContentResolver().openFileDescriptor(docTreeUri, "r");
            assert pfd != null;
            StructStatVfs stats = Os.fstatvfs(pfd.getFileDescriptor());
            double spaceInGigaBytes = stats.f_bavail * stats.f_bsize / 1024.0 / 1024 / 1024;
            pfd.close();
            return spaceInGigaBytes;
        } catch (Exception e) {
            Log.error("[FileUtil] Cannot get storage size.");
        }

        return 0;
    }

    public static void closeQuietly(AutoCloseable closeable) {
        if (closeable != null) {
            try {
                closeable.close();
            } catch (RuntimeException rethrown) {
                throw rethrown;
            } catch (Exception ignored) {
            }
        }
    }
}
