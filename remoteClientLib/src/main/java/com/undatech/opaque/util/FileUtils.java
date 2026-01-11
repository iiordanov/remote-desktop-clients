package com.undatech.opaque.util;

import static com.undatech.opaque.RemoteClientLibConstants.MAX_CONFIG_FILE_SIZE_BYTES;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.net.Uri;
import android.util.Log;

import com.undatech.opaque.RemoteClientLibConstants;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;

public class FileUtils {
    public static String TAG = "FileUtils";

    public static String join(String path1, String path2) {
        File file1 = new File(path1);
        File file2 = new File(file1, path2);
        return file2.getPath();
    }

    public static void deleteFile(String path) {
        new File(path).delete();
    }

    /**
     * Outputs the given InputStream to a file.
     *
     * @param is
     * @param file
     * @throws IOException
     */
    public static boolean outputToFile(InputStream is, File file, int maxBytes) throws IOException {
        BufferedInputStream bis = new BufferedInputStream(is);
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();

        byte[] data = new byte[RemoteClientLibConstants.URL_BUFFER_SIZE];
        int current = 0;
        int total = 0;

        while ((current = bis.read(data, 0, data.length)) != -1) {
            buffer.write(data, 0, current);
            total += current;
            if (total > maxBytes) {
                return false;
            }
        }

        FileOutputStream fos = new FileOutputStream(file);
        fos.write(buffer.toByteArray());
        fos.close();
        return true;
    }

    public static InputStream getInputStreamFromUri(ContentResolver resolver, Uri uri) {
        InputStream in = null;
        try {
            in = resolver.openInputStream(uri);
        } catch (Exception e) {
            Log.e(TAG, "getInputStreamFromUri: uri: " + uri + ", exception: " + e);
            e.printStackTrace();
        }
        return in;
    }

    public static OutputStream getOutputStreamFromUri(ContentResolver resolver, Uri uri) {
        OutputStream out = null;
        try {
            out = resolver.openOutputStream(uri, "wt");
        } catch (IOException e) {
            e.printStackTrace();
        }
        return out;
    }

    public static List<String> listFiles(Context context, String dirFrom) throws IOException {
        Resources res = context.getResources();
        AssetManager am = res.getAssets();
        String fileList[] = am.list(dirFrom);

        if (fileList != null) {
            for (int i = 0; i < fileList.length; i++) {
                Log.d("", fileList[i]);
            }
        }
        return (List<String>) Arrays.asList(fileList);
    }

    public static void logFilesInPrivateStorage(Context context) {
        Log.d(TAG, "logFilesInPrivateStorage");
        for (File f : getListFiles(context.getFilesDir())) {
            Log.d(TAG, f.toString());
        }
    }

    public static List<File> getListFiles(File parentDir) {
        List<File> inFiles = new ArrayList<>();
        Queue<File> files = new LinkedList<>(Arrays.asList(parentDir.listFiles()));
        while (!files.isEmpty()) {
            File file = files.remove();
            if (file.isDirectory()) {
                files.addAll(Arrays.asList(file.listFiles()));
            } else {
                inFiles.add(file);
            }
        }
        return inFiles;
    }

    public static void deletePrivateFileIfExisting(Context context, String path) {
        String privatePath = context.getFilesDir().getPath() + "/" + path;
        File file = new File(privatePath);
        try {
            deleteDirectoryRecursively(file);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static boolean deleteDirectoryRecursively(File file) throws IOException {
        if (file.isDirectory()) {
            File[] entries = file.listFiles();
            if (entries != null) {
                for (File entry : entries) {
                    return deleteDirectoryRecursively(entry);
                }
            }
        }
        if (!file.delete()) {
            Log.d(TAG, "Failed to delete the file or directory: " + file.toString());
            return false;
        }
        Log.d(TAG, "Successfully deleted the file or directory: " + file.toString());
        return true;
    }

    public static void writeDataUriOutToFile(Uri data, String tempConfigFile) throws IOException {
        // Download the file and write it out.
        URL url = new URL(data.toString());
        File file = new File(tempConfigFile);
        URLConnection ucon = url.openConnection();
        FileUtils.outputToFile(ucon.getInputStream(), file, MAX_CONFIG_FILE_SIZE_BYTES);
    }
}
