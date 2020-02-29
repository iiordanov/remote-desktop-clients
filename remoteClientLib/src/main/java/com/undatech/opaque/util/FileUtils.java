package com.undatech.opaque.util;

import java.io.File;

public class FileUtils {
    public static String join(String path1, String path2) {
        File file1 = new File(path1);
        File file2 = new File(file1, path2);
        return file2.getPath();
    }
}
