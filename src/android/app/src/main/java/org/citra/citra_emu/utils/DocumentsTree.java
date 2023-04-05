package org.citra.citra_emu.utils;

import android.content.Context;
import android.net.Uri;
import android.provider.DocumentsContract;

import androidx.annotation.Nullable;
import androidx.documentfile.provider.DocumentFile;

import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.model.CheapDocument;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;
import java.util.StringTokenizer;

/**
 * A cached document tree for citra user directory.
 * For every filepath which is not startsWith "content://" will need to use this class to traverse.
 * For example:
 * C++ citra log file directory will be /log/citra_log.txt.
 * After DocumentsTree.resolvePath() it will become content URI.
 */
public class DocumentsTree {
    private DocumentsNode root;
    private final Context context;
    public static final String DELIMITER = "/";

    public DocumentsTree() {
        context = CitraApplication.getAppContext();
    }

    public void setRoot(Uri rootUri) {
        root = null;
        root = new DocumentsNode();
        root.uri = rootUri;
        root.isDirectory = true;
    }

    public boolean createFile(String filepath, String name) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null) return false;
        if (!node.isDirectory) return false;
        if (!node.loaded) structTree(node);
        Uri mUri = node.uri;
        try {
            String filename = URLDecoder.decode(name, FileUtil.DECODE_METHOD);
            if (node.children.get(filename) != null) return true;
            DocumentFile createdFile = FileUtil.createFile(context, mUri.toString(), name);
            if (createdFile == null) return false;
            DocumentsNode document = new DocumentsNode(createdFile, false);
            document.parent = node;
            node.children.put(document.key, document);
            return true;
        } catch (Exception e) {
            Log.error("[DocumentsTree]: Cannot create file, error: " + e.getMessage());
        }
        return false;
    }

    public boolean createDir(String filepath, String name) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null) return false;
        if (!node.isDirectory) return false;
        if (!node.loaded) structTree(node);
        Uri mUri = node.uri;
        try {
            String filename = URLDecoder.decode(name, FileUtil.DECODE_METHOD);
            if (node.children.get(filename) != null) return true;
            DocumentFile createdDirectory = FileUtil.createDir(context, mUri.toString(), name);
            if (createdDirectory == null) return false;
            DocumentsNode document = new DocumentsNode(createdDirectory, true);
            document.parent = node;
            node.children.put(document.key, document);
            return true;
        } catch (Exception e) {
            Log.error("[DocumentsTree]: Cannot create file, error: " + e.getMessage());
        }
        return false;
    }

    public int openContentUri(String filepath, String openmode) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null) {
            return -1;
        }
        return FileUtil.openContentUri(context, node.uri.toString(), openmode);
    }

    public String getFilename(String filepath) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null) {
            return "";
        }
        return node.name;
    }

    public String[] getFilesName(String filepath) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null || !node.isDirectory) {
            return new String[0];
        }
        // If this directory have not been iterate struct it.
        if (!node.loaded) structTree(node);
        return node.children.keySet().toArray(new String[0]);
    }

    public long getFileSize(String filepath) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null || node.isDirectory) {
            return 0;
        }
        return FileUtil.getFileSize(context, node.uri.toString());
    }

    public boolean isDirectory(String filepath) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null) return false;
        return node.isDirectory;
    }

    public boolean Exists(String filepath) {
        return resolvePath(filepath) != null;
    }

    public boolean copyFile(String sourcePath, String destinationParentPath, String destinationFilename) {
        DocumentsNode sourceNode = resolvePath(sourcePath);
        if (sourceNode == null) return false;
        DocumentsNode destinationNode = resolvePath(destinationParentPath);
        if (destinationNode == null) return false;
        try {
            DocumentFile destinationParent = DocumentFile.fromTreeUri(context, destinationNode.uri);
            if (destinationParent == null) return false;
            String filename = URLDecoder.decode(destinationFilename, "UTF-8");
            DocumentFile destination = destinationParent.createFile("application/octet-stream", filename);
            if (destination == null) return false;
            DocumentsNode document = new DocumentsNode();
            document.uri = destination.getUri();
            document.parent = destinationNode;
            document.name = destination.getName();
            document.isDirectory = destination.isDirectory();
            document.loaded = true;
            InputStream input = context.getContentResolver().openInputStream(sourceNode.uri);
            OutputStream output = context.getContentResolver().openOutputStream(destination.getUri(), "wt");
            byte[] buffer = new byte[1024];
            int len;
            while ((len = input.read(buffer)) != -1) {
                output.write(buffer, 0, len);
            }
            input.close();
            output.flush();
            output.close();
            destinationNode.children.put(document.key, document);
            return true;
        } catch (Exception e) {
            Log.error("[DocumentsTree]: Cannot copy file, error: " + e.getMessage());
        }
        return false;
    }

    public boolean renameFile(String filepath, String destinationFilename) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null) return false;
        try {
            Uri mUri = node.uri;
            String filename = URLDecoder.decode(destinationFilename, FileUtil.DECODE_METHOD);
            DocumentsContract.renameDocument(context.getContentResolver(), mUri, filename);
            node.rename(filename);
            return true;
        } catch (Exception e) {
            Log.error("[DocumentsTree]: Cannot rename file, error: " + e.getMessage());
        }
        return false;
    }

    public boolean deleteDocument(String filepath) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null) return false;
        try {
            Uri mUri = node.uri;
            if (!DocumentsContract.deleteDocument(context.getContentResolver(), mUri)) {
                return false;
            }
            if (node.parent != null) {
                node.parent.children.remove(node.key);
            }
            return true;
        } catch (Exception e) {
            Log.error("[DocumentsTree]: Cannot rename file, error: " + e.getMessage());
        }
        return false;
    }

    @Nullable
    private DocumentsNode resolvePath(String filepath) {
        if (root == null)
            return null;
        StringTokenizer tokens = new StringTokenizer(filepath, DELIMITER, false);
        DocumentsNode iterator = root;
        while (tokens.hasMoreTokens()) {
            String token = tokens.nextToken();
            if (token.isEmpty()) continue;
            iterator = find(iterator, token);
            if (iterator == null) return null;
        }
        return iterator;
    }

    @Nullable
    private DocumentsNode find(DocumentsNode parent, String filename) {
        if (parent.isDirectory && !parent.loaded) {
            structTree(parent);
        }
        return parent.children.get(filename);
    }

    /**
     * Construct current level directory tree
     *
     * @param parent parent node of this level
     */
    private void structTree(DocumentsNode parent) {
        CheapDocument[] documents = FileUtil.listFiles(context, parent.uri);
        for (CheapDocument document : documents) {
            DocumentsNode node = new DocumentsNode(document);
            node.parent = parent;
            parent.children.put(node.key, node);
        }
        parent.loaded = true;
    }

    private static class DocumentsNode {
        private DocumentsNode parent;
        private final Map<String, DocumentsNode> children = new HashMap<>();
        private String key;
        private String name;
        private Uri uri;
        private boolean loaded = false;
        private boolean isDirectory = false;

        private DocumentsNode() {}

        private DocumentsNode(CheapDocument document) {
            name = document.getFilename();
            uri = document.getUri();
            key = FileUtil.getFilenameWithExtensions(uri);
            isDirectory = document.isDirectory();
            loaded = !isDirectory;
        }

        private DocumentsNode(DocumentFile document, boolean isCreateDir) {
            name = document.getName();
            uri = document.getUri();
            key = FileUtil.getFilenameWithExtensions(uri);
            isDirectory = isCreateDir;
            loaded = true;
        }

        private void rename(String key) {
            if (parent == null) {
                return;
            }
            parent.children.remove(this.key);
            this.name = key;
            parent.children.put(key, this);
        }
    }
}
