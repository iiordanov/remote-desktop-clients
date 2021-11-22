package com.iiordanov.util;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.util.Log;
import android.widget.Toast;

import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.Utils;
import com.undatech.remoteClientUi.R;

import java.util.Arrays;

/**
 * Created by iordan on 24/06/18.
 */

public class PermissionsManager {
    public static String TAG = "PermissionsManager";

    private static String[] retrievePermissions(Context context) {
        Log.i(TAG, "Retrieving permissions.");
        try {
            String [] requestedPermissions = context.getPackageManager().getPackageInfo(context
                    .getPackageName(), PackageManager.GET_PERMISSIONS).requestedPermissions;
            android.util.Log.d(TAG, Arrays.toString(requestedPermissions));
            return requestedPermissions;
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("This should have never happened.", e);
        }
    }

    public void requestPermissions(Activity activity, boolean showToast) {
        Log.i(TAG, "Requesting permissions.");
        String[] permissions = retrievePermissions(activity);
        for (String permission: permissions) {
            // Here, thisActivity is the current activity
            if (ContextCompat.checkSelfPermission(activity, permission) != PackageManager.PERMISSION_GRANTED) {
                if (!Utils.querySharedPreferenceBoolean(activity, Constants.permissionsRequested)) {
                    Utils.setSharedPreferenceBoolean(activity, Constants.permissionsRequested, true);
                    // No explanation needed; request the permission
                    ActivityCompat.requestPermissions(activity, permissions,0);
                } else if (showToast) {
                    Toast.makeText(activity, R.string.please_grant_permission_from_prefs, Toast.LENGTH_SHORT).show();
                }
            }
        }
    }
}
