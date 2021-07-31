package com.iiordanov.bVNC;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;

public class GlobalPreferencesActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getSupportFragmentManager().beginTransaction()
                .replace(android.R.id.content, new GlobalPreferencesFragment())
                .commit();
    }
}
