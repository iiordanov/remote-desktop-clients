package com.iiordanov.bVNC;

import java.util.ArrayList;
import java.util.Collections;

import com.iiordanov.bVNC.dialogs.IntroTextDialog;

import android.app.Activity;
import android.graphics.Point;
import android.os.Bundle;
import android.view.Display;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

public abstract class MainConfiguration extends Activity {
    protected ConnectionBean selected;
    protected Database database;
    protected Spinner spinnerConnection;
    protected boolean startingOrHasPaused = true;
    protected boolean isFree;
    
    protected abstract void updateViewFromSelected();

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        System.gc();
        isFree = this.getPackageName().contains("free");
        spinnerConnection = (Spinner)findViewById(R.id.spinnerConnection);
        spinnerConnection.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> ad, View view, int itemIndex, long id) {
                selected = (ConnectionBean)ad.getSelectedItem();
                updateViewFromSelected();
            }
            @Override
            public void onNothingSelected(AdapterView<?> ad) {
                selected = null;
            }
        });
        
        database = new Database(this);
    }
    
    public void arriveOnPage() {
        ArrayList<ConnectionBean> connections = new ArrayList<ConnectionBean>();
        ConnectionBean.getAll(database.getReadableDatabase(),
                              ConnectionBean.GEN_TABLE_NAME, connections,
                              ConnectionBean.newInstance);
        Collections.sort(connections);
        connections.add(0, new ConnectionBean(this));
        int connectionIndex = 0;
        if (connections.size() > 1) {
            MostRecentBean mostRecent = ConnectionBean.getMostRecent(database.getReadableDatabase());
            if (mostRecent != null) {
                for (int i = 1; i < connections.size(); ++i) {
                    if (connections.get(i).get_Id() == mostRecent.getConnectionId()) {
                        connectionIndex = i;
                        break;
                    }
                }
            }
        }
        spinnerConnection.setAdapter(new ArrayAdapter<ConnectionBean>(this, R.layout.connection_list_entry,
                                     connections.toArray(new ConnectionBean[connections.size()])));
        spinnerConnection.setSelection(connectionIndex, false);
        selected = connections.get(connectionIndex);
        updateViewFromSelected();
        IntroTextDialog.showIntroTextIfNecessary(this, database, isFree&&startingOrHasPaused);
        startingOrHasPaused = false;
    }
    
    public Database getDatabaseHelper() {
        return database;
    }
    
    public static boolean isBlackBerry () {
        boolean bb = false;
        if (android.os.Build.MODEL.contains("BlackBerry") ||
            android.os.Build.BRAND.contains("BlackBerry") || 
            android.os.Build.MANUFACTURER.contains("BlackBerry")) {
            bb = true;
        }
        return bb;
    }
    
    /**
     * Returns the display height, or if the device has software
     * buttons, the 'bottom' of the view (in order to take into account the
     * software buttons.
     * @return the height in pixels.
     */
    public int getHeight () {
        View v    = getWindow().getDecorView().findViewById(android.R.id.content);
        Display d = getWindowManager().getDefaultDisplay();
        int bottom = v.getBottom();
        Point outSize = new Point();
        d.getSize(outSize);
        int height = outSize.y;
        int value = height;
        if (android.os.Build.VERSION.SDK_INT >= 14) {
            android.view.ViewConfiguration vc = ViewConfiguration.get(this);
            if (vc.hasPermanentMenuKey())
                value = bottom;
        }
        if (isBlackBerry ()) {
            value = bottom;
        }
        return value;
    }
    
    /**
     * Returns the display width, or if the device has software
     * buttons, the 'right' of the view (in order to take into account the
     * software buttons.
     * @return the width in pixels.
     */
    public int getWidth () {
        View v    = getWindow().getDecorView().findViewById(android.R.id.content);
        Display d = getWindowManager().getDefaultDisplay();
        int right = v.getRight();
        Point outSize = new Point();
        d.getSize(outSize);
        int width = outSize.x;
        if (android.os.Build.VERSION.SDK_INT >= 14) {
            android.view.ViewConfiguration vc = ViewConfiguration.get(this);
            if (vc.hasPermanentMenuKey())
                return right;
        }
        return width;
    }
}
