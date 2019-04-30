package com.iiordanov.bVNC.input;

import android.content.Context;
import android.os.Handler;
import android.os.SystemClock;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import com.iiordanov.bVNC.MetaKeyBean;
import com.iiordanov.bVNC.RemoteCanvas;
import com.undatech.opaque.RfbConnectable;

public abstract class RemoteKeyboard {
    public final static int SCAN_ESC = 1;
    public final static int SCAN_LEFTCTRL = 29;
    public final static int SCAN_LEFTSHIFT = 42;
    public final static int SCAN_RIGHTSHIFT = 54;
    public final static int SCAN_LEFTALT = 56;
    public final static int SCAN_RIGHTCTRL = 97;
    public final static int SCAN_RIGHTALT = 100;
    public final static int SCAN_DELETE = 111;
    public final static int SCAN_LEFTSUPER = 125;
    public final static int SCAN_RIGHTSUPER = 126;
    public final static int SCAN_F1 = 59;
    public final static int SCAN_F2 = 60;
    public final static int SCAN_F3 = 61;
    public final static int SCAN_F4 = 62;
    public final static int SCAN_F5 = 63;
    public final static int SCAN_F6 = 64;
    public final static int SCAN_F7 = 65;
    public final static int SCAN_F8 = 66;
    public final static int SCAN_F9 = 67;
    public final static int SCAN_F10 = 68;
    //public final static int SCAN_HOME = 102;
    //public final static int SCAN_END = 107;
    
    // Useful shortcuts for modifier masks.
    public final static int CTRL_MASK   = KeyEvent.META_SYM_ON;
    public final static int SHIFT_MASK  = KeyEvent.META_SHIFT_ON;
    public final static int ALT_MASK    = KeyEvent.META_ALT_ON;
    public final static int SUPER_MASK  = 0x8;
    public final static int RCTRL_MASK  = KeyEvent.META_CTRL_RIGHT_ON;
    public final static int RSHIFT_MASK = KeyEvent.META_SHIFT_RIGHT_ON;
    public final static int RALT_MASK   = 0x10;
    public final static int RSUPER_MASK = KeyEvent.META_META_RIGHT_ON;

    protected RemoteCanvas canvas;
    protected Handler handler;
    protected RfbConnectable rfb;
    protected Context context;
    protected KeyRepeater keyRepeater;

    // Variable holding the state of any pressed hardware meta keys (Ctrl, Alt...)
    protected int hardwareMetaState = 0;
    
    // Use camera button as meta key for right mouse button
    boolean cameraButtonDown = false;
    
    // Keep track when a seeming key press was the result of a menu shortcut
    int lastKeyDown;
    boolean afterMenu;

    // Variable holding the state of the on-screen buttons for meta keys (Ctrl, Alt...)
    protected int onScreenMetaState = 0;
    
    // Variable holding the state of the last metaState resulting from a button press.
    protected int lastDownMetaState = 0;
    
    // Variable used for BB10 workarounds
    boolean bb = false;
    
    RemoteKeyboard (RfbConnectable r, RemoteCanvas v, Handler h) {
        this.rfb = r;
        this.canvas = v;
        this.handler = h;
        
        keyRepeater = new KeyRepeater (this, h);
        
        if (android.os.Build.MODEL.contains("BlackBerry") ||
            android.os.Build.BRAND.contains("BlackBerry") || 
            android.os.Build.MANUFACTURER.contains("BlackBerry")) {
            bb = true;
        }
    }

    public boolean keyEvent(int keyCode, KeyEvent evt) {
        return processLocalKeyEvent (keyCode, evt, 0);
    }
    
    public abstract boolean processLocalKeyEvent(int keyCode, KeyEvent evt, int additionalMetaState);

    public void repeatKeyEvent(int keyCode, KeyEvent event) { keyRepeater.start(keyCode, event); }

    public void stopRepeatingKeyEvent() { keyRepeater.stop(); }

    public abstract void sendMetaKey(MetaKeyBean meta);
    
    /**
     * Toggles on-screen Ctrl mask. Returns true if result is Ctrl enabled, false otherwise.
     * @return true if on false otherwise.
     */
    public boolean onScreenCtrlToggle()    {
        // If we find Ctrl on, turn it off. Otherwise, turn it on.
        if (onScreenMetaState == (onScreenMetaState | CTRL_MASK)) {
            onScreenCtrlOff();
            return false;
        }
        else {
            onScreenMetaState = onScreenMetaState | CTRL_MASK;
            return true;
        }
    }
    
    /**
     * Turns off on-screen Ctrl.
     */
    public void onScreenCtrlOff()    {
        onScreenMetaState = onScreenMetaState & ~CTRL_MASK;
    }
    
    /**
     * Toggles on-screen Alt mask.  Returns true if result is Alt enabled, false otherwise.
     * @return true if on false otherwise.
     */
    public boolean onScreenAltToggle() {
        // If we find Alt on, turn it off. Otherwise, turn it on.
        if (onScreenMetaState == (onScreenMetaState | ALT_MASK)) {
            onScreenAltOff();
            return false;
        }
        else {
            onScreenMetaState = onScreenMetaState | ALT_MASK;
            return true;
        }
    }

    /**
     * Turns off on-screen Alt.
     */
    public void onScreenAltOff()    {
        onScreenMetaState = onScreenMetaState & ~ALT_MASK;
    }

    /**
     * Toggles on-screen Super mask.  Returns true if result is Super enabled, false otherwise.
     * @return true if on false otherwise.
     */
    public boolean onScreenSuperToggle() {
        // If we find Super on, turn it off. Otherwise, turn it on.
        if (onScreenMetaState == (onScreenMetaState | SUPER_MASK)) {
            onScreenSuperOff();
            return false;
        }
        else {
            onScreenMetaState = onScreenMetaState | SUPER_MASK;
            return true;
        }
    }
    
    /**
     * Turns off on-screen Super.
     */
    public void onScreenSuperOff() {
        onScreenMetaState = onScreenMetaState & ~SUPER_MASK;        
    }
    
    /**
     * Toggles on-screen Shift mask.  Returns true if result is Shift enabled, false otherwise.
     * @return true if on false otherwise.
     */
    public boolean onScreenShiftToggle() {
        // If we find Super on, turn it off. Otherwise, turn it on.
        if (onScreenMetaState == (onScreenMetaState | SHIFT_MASK)) {
            onScreenShiftOff();
            return false;
        }
        else {
            onScreenMetaState = onScreenMetaState | SHIFT_MASK;
            return true;
        }
    }

    /**
     * Turns off on-screen Shift.
     */
    public void onScreenShiftOff() {
        onScreenMetaState = onScreenMetaState & ~SHIFT_MASK;
    }

    public int getMetaState () {
        return onScreenMetaState|lastDownMetaState;
    }
    
    public void setAfterMenu(boolean value) {
        afterMenu = value;
    }
    
    public boolean getCameraButtonDown() {
        return cameraButtonDown;
    }
    
    public void clearMetaState () {
        onScreenMetaState = 0;
    }
    
    public void sendText(String s) {
        for (int i = 0; i < s.length(); i++) {
            KeyEvent event = null;
            char c = s.charAt(i);
            if (Character.isISOControl(c)) {
                if (c == '\n') {
                    int keyCode = KeyEvent.KEYCODE_ENTER;
                    keyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
                    try { Thread.sleep(10); } catch (InterruptedException e) { }
                    keyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_UP, keyCode));
                }
            } else {
                event = new KeyEvent(SystemClock.uptimeMillis(), s.substring(i, i+1), KeyCharacterMap.FULL, 0);
                keyEvent(event.getKeyCode(), event);
                try { Thread.sleep(10); } catch (InterruptedException e) { }
            }
        }
    }
    
    public void sendKeySym (int keysym, int metaState) {
        char c = (char)XKeySymCoverter.keysym2ucs(keysym);
        sendUnicode(c, metaState);
    }
    
    /**
     * Tries to convert a unicode character to a KeyEvent and if successful sends with keyEvent().
     * @param unicodeChar
     * @param additionalMetaState
     */
    public boolean sendUnicode (char unicodeChar, int additionalMetaState) {
        KeyCharacterMap fullKmap    = KeyCharacterMap.load(KeyCharacterMap.FULL);
        KeyCharacterMap virtualKmap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
        char[] s = new char[1];
        s[0] = unicodeChar;
        
        KeyEvent[] events = fullKmap.getEvents(s);
        // Failing with the FULL keymap, try the VIRTUAL_KEYBOARD one.
        if (events == null) {
            events = virtualKmap.getEvents(s);
        }
        
        if (events != null) {
            for (int i = 0; i < events.length; i++) {
                KeyEvent evt = events[i];
                processLocalKeyEvent(evt.getKeyCode(), evt, additionalMetaState);
                KeyEvent upEvt = new KeyEvent(KeyEvent.ACTION_UP, evt.getKeyCode());
                processLocalKeyEvent(upEvt.getKeyCode(), upEvt, additionalMetaState);
                return true;
            }
        } else {
            android.util.Log.e("RemoteKeyboard", "Could not use any keymap to generate KeyEvent for unicode: " + unicodeChar);
        }
        return false;
    }
    
    /**
     * Converts event meta state to our meta state.
     * @param event
     * @return
     */
    protected int convertEventMetaState (KeyEvent event) {
        return convertEventMetaState(event, event.getMetaState());
    }
    
    /**
     * Converts event meta state to our meta state.
     * @param event
     * @return
     */
    protected int convertEventMetaState (KeyEvent event, int eventMetaState) {
        int metaState = 0;
        int leftAltMetaStateMask = 0;
        // Detect whether this event is coming from a default hardware keyboard.
        // We have to leave KeyEvent.KEYCODE_ALT_LEFT for symbol input on a default hardware keyboard.
        boolean defaultHardwareKbd = (event.getScanCode() != 0 && event.getDeviceId() == 0);
        if (!bb && !defaultHardwareKbd) {
            leftAltMetaStateMask = 0x10 /*KeyEvent.META_ALT_LEFT_ON*/;
        }
        
        // Add shift, ctrl, alt, and super to metaState if necessary.
        if ((eventMetaState & (0x00000040 | 1) /*KeyEvent.META_SHIFT_LEFT_ON/META_SHIFT_ON*/) != 0) {
            metaState |= SHIFT_MASK;
        }
        if ((eventMetaState & 0x00000080 /*KeyEvent.META_SHIFT_RIGHT_ON*/) != 0) {
            metaState |= RSHIFT_MASK;
        }
        if ((eventMetaState & 0x00002000 /*KeyEvent.META_CTRL_LEFT_ON*/) != 0) {
            metaState |= CTRL_MASK;
        }
        if ((eventMetaState & 0x00004000 /*KeyEvent.META_CTRL_RIGHT_ON*/) != 0) {
            metaState |= RCTRL_MASK;
        }
        if ((eventMetaState & leftAltMetaStateMask) !=0) {
            metaState |= ALT_MASK;
        }
        if ((eventMetaState & 0x00000020 /*KeyEvent.META_ALT_RIGHT_ON*/) !=0) {
            metaState |= RALT_MASK;
        }
        if ((eventMetaState & 0x00020000 /*KeyEvent.META_META_LEFT_ON*/) != 0) {
            metaState |= SUPER_MASK;
        }
        if ((eventMetaState & 0x00040000 /*KeyEvent.META_META_RIGHT_ON*/) != 0) {
            metaState |= RSUPER_MASK;
        }
        
        return metaState;
    }
}
