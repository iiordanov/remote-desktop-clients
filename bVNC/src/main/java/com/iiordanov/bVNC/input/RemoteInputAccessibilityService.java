/**
 * Copyright (C) 2026 Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * <p>
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <p>
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC.input;

import static android.provider.Settings.ACTION_ACCESSIBILITY_SETTINGS;
import static com.iiordanov.bVNC.Constants.PREF_ACCESSIBILITY_SERVICE_PROMPT_SHOWN;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MenuItem;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import com.google.android.material.snackbar.Snackbar;
import com.iiordanov.bVNC.Utils;
import com.undatech.remoteClientUi.R;

/**
 * Accessibility service that intercepts physical key events before the Android OS
 * can consume system key combinations (e.g. Alt+Tab). When a remote session is active
 * and a KeyInterceptListener is registered, all key events are routed to that listener
 * instead of being delivered to the focused window via the normal dispatch path.
 * The listener must return true to consume the event (preventing normal delivery)
 * or false to let it pass through.
 */
@SuppressLint("AccessibilityPolicy")
public class RemoteInputAccessibilityService extends AccessibilityService {
    private static final String TAG = "RemoteInp...lityService";
    /**
     * Receives key events captured by the accessibility service.
     */
    public interface KeyInterceptListener {
        /**
         * Called for each physical key event before it is delivered to the focused window.
         *
         * @param event the key event
         * @return true to consume the event (prevent OS and app from handling it normally),
         * false to let it pass through unchanged
         */
        boolean onInterceptedKeyEvent(KeyEvent event);
    }

    private static volatile KeyInterceptListener interceptListener;

    /**
     * Registers the listener that will receive all intercepted key events.
     * Pass null to stop interception.
     */
    public static void setKeyInterceptListener(KeyInterceptListener listener) {
        interceptListener = listener;
    }

    /**
     * Returns true if the accessibility service is enabled in system settings for this app.
     */
    public static boolean isEnabled(Context context) {
        String enabledServices = Settings.Secure.getString(context.getContentResolver(), Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (TextUtils.isEmpty(enabledServices)) {
            return false;
        }
        String componentName = context.getPackageName() + "/" + RemoteInputAccessibilityService.class.getName();
        for (String service : enabledServices.split(":")) {
            if (service.equalsIgnoreCase(componentName)) {
                return true;
            }
        }
        return false;
    }

    public static void promptForAccessibilityServiceIfNeeded(Context context) {
        if (isEnabled(context)) {
            return;
        }
        if (Utils.querySharedPreferenceBoolean(context,
                PREF_ACCESSIBILITY_SERVICE_PROMPT_SHOWN, false)) {
            return;
        }
        Utils.setSharedPreferenceBoolean(context,
                PREF_ACCESSIBILITY_SERVICE_PROMPT_SHOWN, true);
        String appName = context.getString(R.string.accessibility_service_label);
        String message = context.getString(R.string.accessibility_service_prompt_message);
        new AlertDialog.Builder(context)
                .setTitle(R.string.accessibility_service_prompt_title)
                .setMessage(message)
                .setPositiveButton(R.string.accessibility_service_prompt_go_to_settings,
                        (d, w) -> context.startActivity(new Intent(ACTION_ACCESSIBILITY_SETTINGS)))
                .setNegativeButton(R.string.accessibility_service_prompt_not_now, null)
                .show();
    }

    public static void enableOrReportKeyboardShortcutCapture(Context context, View view) {
        Log.i(TAG, "enableKeyboardShortcutCapture");
        if (isEnabled(context)) {
            Snackbar.make(view, R.string.accessibility_service_already_enabled, Snackbar.LENGTH_LONG).show();
        } else {
            context.startActivity(new Intent(ACTION_ACCESSIBILITY_SETTINGS));
        }
    }

    @Override
    protected void onServiceConnected() {
        // Override the XML declaration to the absolute minimum required: key event filtering
        // only, with no accessibility events, and only from the app package itself.
        // This ensures the service cannot observe any screen content, window changes, or user
        // interactions beyond the key events it explicitly needs to forward to the remote session.
        AccessibilityServiceInfo info = getServiceInfo();
        info.packageNames = new String[]{Utils.pName(getBaseContext())};
        info.eventTypes = 0;
        info.flags = AccessibilityServiceInfo.FLAG_REQUEST_FILTER_KEY_EVENTS;
        setServiceInfo(info);
    }

    @Override
    protected boolean onKeyEvent(KeyEvent event) {
        if (interceptListener != null) {
            return interceptListener.onInterceptedKeyEvent(event);
        }
        return false;
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        // Not used — this service only captures key events.
    }

    @Override
    public void onInterrupt() {
        // Not used.
    }
}
