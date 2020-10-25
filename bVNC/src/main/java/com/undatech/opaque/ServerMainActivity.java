package com.undatech.opaque;

import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.support.v4.app.FragmentActivity;
import android.view.View;

import android.view.Menu;
import android.view.MenuItem;

import com.undatech.remoteClientUi.R;
import com.undatech.opaque.NativeLib;

public class ServerMainActivity extends FragmentActivity {

    private Context appContext;
    private Integer someInt;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appContext = getApplicationContext();
        setContentView(R.layout.activity_servermain);
        NativeLib somenativelib = new NativeLib();
        someInt = somenativelib.add(1, 2);
        android.util.Log.d("SOMEINT", "Here is some int=" + someInt.toString());

    }
}