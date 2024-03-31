package com.undatech.opaque.util;

import static android.content.Context.UI_MODE_SERVICE;

import android.app.UiModeManager;
import android.content.Context;
import android.content.res.Configuration;
import android.util.Log;

public class GeneralUtils {
    public static final String TAG = "GeneralUtils";

    public static Class<?> getClassByName(String name) {
        Class<?> remoteCanvasActivityClass = null;
        try {
            remoteCanvasActivityClass = Class.forName(name);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
        return remoteCanvasActivityClass;
    }

    public static void debugLog(boolean enabled, String tag, String message) {
        if (enabled) {
            android.util.Log.d(tag, message);
        }
    }

    public static boolean isTv(Context context) {
        UiModeManager uiModeManager = (UiModeManager) context.getSystemService(UI_MODE_SERVICE);
        if (uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION) {
            Log.d(TAG, "App is running on Android TV");
            return true;
        } else {
            Log.d(TAG, "App is not running on Android TV");
            return false;
        }
    }
}
