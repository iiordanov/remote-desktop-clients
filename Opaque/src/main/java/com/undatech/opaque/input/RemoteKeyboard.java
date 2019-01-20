/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */


package com.undatech.opaque.input;

import android.content.Context;
import android.os.Handler;
import android.os.SystemClock;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import com.undatech.opaque.RemoteCanvas;
import com.undatech.opaque.SpiceCommunicator;

public abstract class RemoteKeyboard {
    private static final String TAG = "RemoteKeyboard";

    public final static int SCANCODE_LCTRL = 29;
    public final static int SCANCODE_RCTRL = 97;
    
    // My mask for the modifier masks.
    public final static int SHIFT_ON_MASK = 1;
    public final static int ALT_ON_MASK   = 2;
    public final static int CTRL_ON_MASK  = 4;
    public final static int SUPER_ON_MASK = 8;
    public final static int ALTGR_ON_MASK = 16;
    
    protected RemoteCanvas canvas;
    protected Handler handler;
    protected SpiceCommunicator spicecomm;
    protected Context context;
    protected KeyRepeater keyRepeater;

    // Variable holding the state of any pressed hardware meta keys
    protected int hardwareMetaState = 0;
    
    // Variable holding the state of the on-screen meta keys
    protected int onScreenMetaState = 0;
    
    // Variable used for BB10 workarounds
    boolean bb = false;
    
    // Variable holding the state of the last metaState sent over with
    // a button down event. It is reset to 0 with a button up event.
    protected int lastDownMetaState = 0;
    
    RemoteKeyboard (SpiceCommunicator r, RemoteCanvas v, Handler h) {
        spicecomm = r;
        canvas = v;
        handler = h;
        keyRepeater = new KeyRepeater (this, h);
        
        if (android.os.Build.MODEL.contains("BlackBerry") ||
            android.os.Build.BRAND.contains("BlackBerry") || 
            android.os.Build.MANUFACTURER.contains("BlackBerry")) {
            bb = true;
        }
    }

    /**
     * Sends key event to server with no additional meta state.
     * @param keyCode
     * @param event
     * @return
     */
    public abstract boolean keyEvent(int keyCode, KeyEvent event);

    /**
     * Sends key event with additional meta state to server
     * @param keyCode
     * @param event
     * @param additionalMetaState
     * @return
     */
    public abstract boolean keyEvent(int keyCode, KeyEvent event, int additionalMetaState);

    /**
     * Starts repeating a key event.
     * @param keyCode
     * @param event
     */
    public void repeatKeyEvent(int keyCode, KeyEvent event) { keyRepeater.start(keyCode, event); }

    /**
     * Stops repeating the last key event being repeated.
     */
    public void stopRepeatingKeyEvent() { keyRepeater.stop(); }
    
    /**
     * Toggles on-screen Ctrl mask. Returns true if result is Ctrl enabled, false otherwise.
     * @return true if on false otherwise.
     */
    public boolean onScreenCtrlToggle() {
        // If we find Ctrl on, turn it off. Otherwise, turn it on.
        if (onScreenMetaState == (onScreenMetaState | CTRL_ON_MASK)) {
            onScreenCtrlOff();
            return false;
        }
        else {
            onScreenCtrlOn();
            return true;
        }
    }

    /**
     * Turns on on-screen Ctrl.
     */
    public void onScreenCtrlOn() {
        onScreenMetaState = onScreenMetaState | CTRL_ON_MASK;
    }
    
    /**
     * Turns off on-screen Ctrl.
     */
    public void onScreenCtrlOff() {
        onScreenMetaState = onScreenMetaState & ~CTRL_ON_MASK;
    }
    
    /**
     * Toggles on-screen Alt mask.  Returns true if result is Alt enabled, false otherwise.
     * @return true if on false otherwise.
     */
    public boolean onScreenAltToggle() {
        // If we find Alt on, turn it off. Otherwise, turn it on.
        if (onScreenMetaState == (onScreenMetaState | ALT_ON_MASK)) {
            onScreenAltOff();
            return false;
        }
        else {
            onScreenAltOn();
            return true;
        }
    }

    /**
     * Turns on on-screen Alt.
     */
    public void onScreenAltOn() {
        onScreenMetaState = onScreenMetaState | ALT_ON_MASK;
    }
    
    /**
     * Turns off on-screen Alt.
     */
    public void onScreenAltOff() {
        onScreenMetaState = onScreenMetaState & ~ALT_ON_MASK;
    }

    /**
     * Toggles on-screen Super mask.  Returns true if result is Super enabled, false otherwise.
     * @return true if on false otherwise.
     */
    public boolean onScreenSuperToggle() {
        // If we find Super on, turn it off. Otherwise, turn it on.
        if (onScreenMetaState == (onScreenMetaState | SUPER_ON_MASK)) {
            onScreenSuperOff();
            return false;
        }
        else {
            onScreenSuperOn();
            return true;
        }
    }
    
    /**
     * Turns on on-screen Super.
     */
    public void onScreenSuperOn() {
        onScreenMetaState = onScreenMetaState | SUPER_ON_MASK;
    }    
    
    /**
     * Turns off on-screen Super.
     */
    public void onScreenSuperOff() {
        onScreenMetaState = onScreenMetaState & ~SUPER_ON_MASK;
    }
    
    /**
     * Toggles on-screen Shift mask.  Returns true if result is Shift enabled, false otherwise.
     * @return true if on false otherwise.
     */
    public boolean onScreenShiftToggle() {
        // If we find Super on, turn it off. Otherwise, turn it on.
        if (onScreenMetaState == (onScreenMetaState | SHIFT_ON_MASK)) {
            onScreenShiftOff();
            return false;
        }
        else {
            onScreenShiftOn();
            return true;
        }
    }
    
    /**
     * Turns on on-screen Shift.
     */
    public void onScreenShiftOn() {
        onScreenMetaState = onScreenMetaState | SHIFT_ON_MASK;
    }
    
    /**
     * Turns off on-screen Shift.
     */
    public void onScreenShiftOff() {
        onScreenMetaState = onScreenMetaState & ~SHIFT_ON_MASK;
    }
    
    /**
     * Getter for the on-screen meta state.
     * @return
     */
    public int getMetaState () {
        return onScreenMetaState|lastDownMetaState;
    }
    
    /**
     * Clears the on-screen meta state.
     */
    public void clearOnScreenMetaState () {
        onScreenMetaState = 0;
    }

    /**
     * Used to send a string over as a stream of unicode characters.
     * @param s
     */
    public void sendString(String s) {
        for (int i = 0; i < s.length(); i++) {
            char nextChar = s.charAt(i);
            if (Character.isISOControl(nextChar)) {
                if (nextChar == '\n') {
                    int keyCode = KeyEvent.KEYCODE_ENTER;
                    keyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
                    keyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_UP, keyCode));
                }
            } else {
                sendUnicodeChar (nextChar);
            }
        }
    }
    
    /**
     * Tries to convert a unicode character to a KeyEvent and if successful sends with keyEvent().
     * @param unicodeChar
     * @param metaState
     */
    public boolean sendUnicodeChar (char unicodeChar) {
        KeyCharacterMap fullKmap    = KeyCharacterMap.load(KeyCharacterMap.FULL);
        KeyCharacterMap virtualKmap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
        char[] s = new char[1];
        s[0] = unicodeChar;
        KeyEvent[] events = fullKmap.getEvents(s);
        // Failing with the FULL keymap, try the VIRTUAL_KEYBOARD one.
        if (events == null) {
            events = virtualKmap.getEvents(s);
        }
        
        boolean result = false;
        if (events != null) {
            for (int i = 0; i < events.length; i++) {
                KeyEvent evt = events[i];
                keyEvent(evt.getKeyCode(), evt);
                result = true;
            }
        } else {
            android.util.Log.e("RemoteKeyboard", "Could not use any keymap to generate KeyEvent for unicode: " + unicodeChar);
        }
        return result;
    }
    
    /**
     * Converts event meta state to our meta state.
     * @param event
     * @return
     */
    protected int convertEventMetaState (KeyEvent event, int eventMetaState) {
        int metaState = 0;
        int altMask = KeyEvent.META_ALT_RIGHT_ON;
        // Detect whether this event is coming from a default hardware keyboard.
        // We have to leave KeyEvent.KEYCODE_ALT_LEFT for symbol input on a default hardware keyboard.
        boolean defaultHardwareKbd = (event.getDeviceId() == 0);
        if (!bb && !defaultHardwareKbd) {
            altMask = KeyEvent.META_ALT_MASK;
        }
        
        // Add shift, ctrl, alt, and super to metaState if necessary.
        if ((eventMetaState & 0x000000c1 /*KeyEvent.META_SHIFT_MASK*/) != 0) {
            metaState |= SHIFT_ON_MASK;
        }
        if ((eventMetaState & 0x00007000 /*KeyEvent.META_CTRL_MASK*/) != 0) {
            metaState |= CTRL_ON_MASK;
        }
        if ((eventMetaState & altMask) !=0) {
            metaState |= ALT_ON_MASK;
        }
        if ((eventMetaState & 0x00070000 /*KeyEvent.META_META_MASK*/) != 0) {
            metaState |= SUPER_ON_MASK;
        }
        return metaState;
    }
}
