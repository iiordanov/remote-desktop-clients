package com.undatech.opaque.util;

import com.iiordanov.bVNC.Constants;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by iordan on 24/06/18.
 */

public class LogcatReader {
    public static String TAG = "LogcatReader";
    private List<String> logcatCommand = new ArrayList<String>();

    public LogcatReader() {
        int id = android.os.Process.myPid();
        logcatCommand.add("logcat");
        logcatCommand.add("-d");
        logcatCommand.add("--pid");
        logcatCommand.add(Integer.toString(id));
    }

    public String getMyLogcat(int lines) {
        ArrayList<String> logCatLines = new ArrayList<>(Constants.LOGCAT_MAX_LINES);
        StringBuilder logCatOutput = new StringBuilder(Constants.LOGCAT_MAX_LINES);
        String line;

        try {
            Process p = new ProcessBuilder().command(logcatCommand).start();
            BufferedReader buffReader = new BufferedReader(new InputStreamReader(p.getInputStream()));
            while ((line = buffReader.readLine()) != null) {
                logCatLines.add(line + "\n");
            }
        } catch (IOException e) {
            android.util.Log.e(TAG, "Error obtaining output from logcat");
            e.printStackTrace();
        }

        for (int i = Math.max(logCatLines.size() - Constants.LOGCAT_MAX_LINES, 0); i < logCatLines.size(); i++) {
            logCatOutput.append(logCatLines.get(i));
        }

        return logCatOutput.toString();
    }
}
