package com.gstreamer;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.content.Context;
import android.content.res.AssetManager;

public class GStreamer {
    private static native void nativeInit(Context context) throws Exception;

    public static void init(Context context) throws Exception {
        nativeInit(context);
        copyFonts(context);
    }

    private static void copyFonts(Context context) {
        AssetManager assetManager = context.getAssets();
        File filesDir = context.getFilesDir();
        File fontsFCDir = new File (filesDir, "fontconfig");
        File fontsDir = new File (fontsFCDir, "fonts");
        File fontsCfg = new File (fontsFCDir, "fonts.conf");

        fontsDir.mkdirs();

        try {
            /* Copy the config file */
            copyFile (assetManager, "fontconfig/fonts.conf", fontsCfg);
            /* Copy the fonts */
            for(String filename : assetManager.list("fontconfig/fonts/truetype")) {
                File font = new File(fontsDir, filename);
                copyFile (assetManager, "fontconfig/fonts/truetype/" + filename, font);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private static void copyFile(AssetManager assetManager, String assetPath, File outFile) throws IOException {
        InputStream in;
        OutputStream out;
        byte[] buffer = new byte[1024];
        int read;

        if (outFile.exists())
            return;

        in = assetManager.open(assetPath);
        out = new FileOutputStream (outFile);
        while((read = in.read(buffer)) != -1){
          out.write(buffer, 0, read);
        }
        in.close();
        out.flush();
        out.close();
   }
}
