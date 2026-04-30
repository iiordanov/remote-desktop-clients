package com.iiordanov.util;

import android.util.Log;

import org.yaml.snakeyaml.Yaml;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Map;

public class CustomClientConfigFileReader {
    private final static String TAG = "ConfigFileReader";

    private final Map<String, Map<String, Map<String, ?>>> configData;

    public CustomClientConfigFileReader(InputStream configFileInputStream) {
        BufferedReader reader;
        reader = new BufferedReader(new InputStreamReader(configFileInputStream));

        Yaml yaml = new Yaml();
        configData = yaml.load(reader);

        try {
            reader.close();
        } catch (IOException e) {
            Log.e(TAG, "Error closing config reader.");
            Log.e(TAG, Log.getStackTraceString(e));
        }
    }

    public Map<String, Map<String, Map<String, ?>>> getConfigData() {
        return configData;
    }
}
