package com.iiordanov.bVNC;

import android.os.Bundle;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import com.trinity.android.apiclient.utils.ClientAPISettings;
import com.trinity.android.apiclient.utils.Utils;
import com.undatech.remoteClientUi.R;

public class GlobalPreferencesFragment extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getPreferenceManager().setSharedPreferencesName(Constants.generalSettingsTag);
        setPreferencesFromResource(R.xml.global_preferences, s);
        if (!Utils.isLocal()) {
            Preference allowInsecure = findPreference(ClientAPISettings.ALLOW_INSECURE_PREF_KEY);
            Preference apiServer = findPreference(ClientAPISettings.API_SERVER_PREF_KEY);
            PreferenceScreen preferenceScreen = getPreferenceScreen();
            preferenceScreen.removePreference(allowInsecure);
            preferenceScreen.removePreference(apiServer);
        }
    }
}