package com.iiordanov.bVNC;

import android.app.ActionBar;
import android.os.Bundle;
import android.util.Log;

import com.iiordanov.util.CustomClientConfigFileReader;

import java.io.IOException;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;

public class CustomVnc extends bVNC {
    private final static String TAG = "CustomVnc";

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        try {
            String packageName = Utils.pName(this);
            CustomClientConfigFileReader configFileReader = new CustomClientConfigFileReader(
                    getAssets().open(packageName + ".yaml"));
            Map<String, Map<String, Map<String, ?>>> configData = configFileReader.getConfigData();

            Utils.setVisibilityForViewElementsViaConfig(this, App.getConfigFileReader().getConfigData(),"mainConfiguration", findViewById(android.R.id.content).getRootView());

            Locale current = getResources().getConfiguration().locale;
            String currentLanguage = current.getLanguage();

            String title = (String) Objects.requireNonNull(Objects.requireNonNull(configData.get("mainConfiguration")).get("title")).get("default");
            String subtitle = (String) Objects.requireNonNull(Objects.requireNonNull(configData.get("mainConfiguration")).get("subtitle")).get("default");
            String currentLanguageTitle = (String) Objects.requireNonNull(Objects.requireNonNull(configData.get("mainConfiguration")).get("title")).get(currentLanguage);
            if (currentLanguageTitle != null) {
                title = currentLanguageTitle;
            }
            String currentLanguageSubTitle = (String) Objects.requireNonNull(Objects.requireNonNull(configData.get("mainConfiguration")).get("subtitle")).get(currentLanguage);
            if (currentLanguageSubTitle != null) {
                subtitle = currentLanguageSubTitle;
            }
            ActionBar ab = getActionBar();
            Objects.requireNonNull(ab).setTitle(title);
            ab.setSubtitle(subtitle);
        } catch (IOException|NullPointerException e) {
            Log.e(TAG, "Error opening config file from assets.");
            Log.e(TAG, Log.getStackTraceString(e));
        }
    }
}
