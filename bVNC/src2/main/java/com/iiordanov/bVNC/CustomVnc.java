package com.iiordanov.bVNC;

import android.os.Bundle;
import android.view.View;
import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.HakkoHmiVnc.*;
import com.iiordanov.util.CustomClientConfigFileReader;

import org.yaml.snakeyaml.Yaml;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Map;

public class CustomVnc extends bVNC {
    private final static String TAG = "CustomVnc";
    private View view;

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        try {
            CustomClientConfigFileReader configFileReader = new CustomClientConfigFileReader(
                            getAssets().open("custom_vnc_client.yaml"));
            Map<String, Integer> data = configFileReader.getConfigData();

            for (String s : data.keySet()){
                android.util.Log.d(TAG, s);
                int resID = getResources().getIdentifier(s, "id", getPackageName());
                view = findViewById(resID);
                view.setVisibility(data.get(s));
            }
        } catch (IOException e) {
            android.util.Log.e(TAG, "Error opening config file from assets.");
            e.printStackTrace();
        }
    }
}
