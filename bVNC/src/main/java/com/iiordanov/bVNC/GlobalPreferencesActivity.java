package com.iiordanov.bVNC;

import android.os.Bundle;

import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;

public class GlobalPreferencesActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ActionBar bar = getSupportActionBar();
        if (bar != null) {
            bar.hide();
        }
        getSupportFragmentManager().beginTransaction()
                .replace(android.R.id.content, new GlobalPreferencesFragment())
                .commit();
    }
}
