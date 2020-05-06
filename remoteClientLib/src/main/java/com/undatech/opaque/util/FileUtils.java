package com.undatech.opaque.util;

import com.undatech.opaque.RemoteClientLibConstants;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class FileUtils {
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
     * @param is
     * @param file
     * @throws IOException
     */
    public static void outputToFile (InputStream is, File file) throws IOException {
        BufferedInputStream bis = new BufferedInputStream(is);
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();

        byte[] data = new byte[RemoteClientLibConstants.URL_BUFFER_SIZE];
        int current = 0;

        while((current = bis.read(data, 0, data.length)) != -1){
            buffer.write(data, 0, current);
        }

        FileOutputStream fos = new FileOutputStream(file);
        fos.write(buffer.toByteArray());
        fos.close();
    }

}
