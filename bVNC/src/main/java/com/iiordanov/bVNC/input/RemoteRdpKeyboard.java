package com.iiordanov.bVNC.input;

import android.os.Handler;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import com.iiordanov.bVNC.App;
import com.iiordanov.bVNC.RemoteCanvas;
import com.undatech.opaque.RfbConnectable;
import com.undatech.opaque.input.RdpKeyboardMapper;
import com.undatech.opaque.util.GeneralUtils;

public class RemoteRdpKeyboard extends RemoteKeyboard {
    private final static String TAG = "RemoteRdpKeyboard";
    protected RdpKeyboardMapper keyboardMapper;
    protected RemoteCanvas canvas;

    public RemoteRdpKeyboard (RfbConnectable r, RemoteCanvas v, Handler h, boolean debugLog) {
        super(r, v.getContext(), h, debugLog);
        canvas = v;
        keyboardMapper = new RdpKeyboardMapper();
        keyboardMapper.init(context);
        keyboardMapper.reset((RdpKeyboardMapper.KeyProcessingListener)r);
    }
    
    public boolean processLocalKeyEvent(int keyCode, KeyEvent evt, int additionalMetaState) {
        GeneralUtils.debugLog(App.debugLog, TAG, "processLocalKeyEvent: " + evt.toString() + " " + keyCode);

        if (rfb != null && rfb.isInNormalProtocol()) {
            RemotePointer pointer = canvas.getPointer();
            boolean down = (evt.getAction() == KeyEvent.ACTION_DOWN) ||
                           (evt.getAction() == KeyEvent.ACTION_MULTIPLE);            
            int metaState = additionalMetaState | convertEventMetaState (evt);

            if (keyCode == KeyEvent.KEYCODE_MENU)
                return true;                           // Ignore menu key

            if (pointer.hardwareButtonsAsMouseEvents(keyCode, evt, metaState|onScreenMetaState|hardwareMetaState))
                return true;

            // Detect whether this event is coming from a default hardware keyboard.
            boolean defaultHardwareKbd = (evt.getDeviceId() == 0);
            if (!down) {
                switch (evt.getScanCode()) {
                case SCAN_LEFTCTRL:
                case SCAN_RIGHTCTRL:
                    hardwareMetaState &= ~CTRL_MASK;
                    break;
                }
                
                switch(keyCode) {
                case KeyEvent.KEYCODE_DPAD_CENTER:
                    hardwareMetaState &= ~CTRL_MASK;
                    break;
                }
            } else {
                // Look for standard scan-codes from hardware keyboards
                switch (evt.getScanCode()) {
                case SCAN_LEFTCTRL:
                case SCAN_RIGHTCTRL:
                    hardwareMetaState |= CTRL_MASK;
                    break;
                }
                
                switch(keyCode) {
                case KeyEvent.KEYCODE_DPAD_CENTER:
                    hardwareMetaState |= CTRL_MASK;
                    break;
                }
            }

            // Update the meta-state with writeKeyEvent.
            metaState = onScreenMetaState|hardwareMetaState|metaState;
            rfb.writeKeyEvent(keyCode, metaState, down);
            if (down) {
                lastDownMetaState = metaState;
            } else {
                lastDownMetaState = 0;
            }

            if (keyCode == 0 /*KEYCODE_UNKNOWN*/) {
                String s = evt.getCharacters();
                GeneralUtils.debugLog(App.debugLog, TAG, "processLocalKeyEvent: getCharacters: " + s);

                if (s != null) {
                    int numchars = s.length();
                    for (int i = 0; i < numchars; i++) {
                        KeyEvent event = new KeyEvent(evt.getEventTime(), s.substring(i, i+1), KeyCharacterMap.FULL, 0);
                        keyboardMapper.processAndroidKeyEvent(event);
                    }
                }
                return true;
            } else {
                // Send the key to be processed through the KeyboardMapper.
                return keyboardMapper.processAndroidKeyEvent(evt);
            }
        } else {
            return false;
        }
    }

    public void sendMetaKey(MetaKeyBean meta) {
        RemotePointer pointer = canvas.getPointer();
        int x = pointer.getX();
        int y = pointer.getY();
        
        if (meta.isMouseClick()) {
            //android.util.Log.i("RemoteRdpKeyboard", "is a mouse click");
            int button = meta.getMouseButtons();
            switch (button) {
            case RemoteVncPointer.MOUSE_BUTTON_LEFT:
                pointer.leftButtonDown(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState);
                break;
            case RemoteVncPointer.MOUSE_BUTTON_RIGHT:
                pointer.rightButtonDown(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState);
                break;
            case RemoteVncPointer.MOUSE_BUTTON_MIDDLE:
                pointer.middleButtonDown(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState);
                break;
            case RemoteVncPointer.MOUSE_BUTTON_SCROLL_UP:
                pointer.scrollUp(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState);
                break;
            case RemoteVncPointer.MOUSE_BUTTON_SCROLL_DOWN:
                pointer.scrollDown(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState);
                break;
            }
            try { Thread.sleep(50); } catch (InterruptedException e) {}
            pointer.releaseButton(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState);

            //rfb.writePointerEvent(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState, button);
            //rfb.writePointerEvent(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState, 0);
        } else if (meta.equals(MetaKeyBean.keyCtrlAltDel)) {
            // TODO: I should not need to treat this specially anymore.
            int savedMetaState = onScreenMetaState|hardwareMetaState;
            // Update the metastate
            rfb.writeKeyEvent(0, RemoteKeyboard.CTRL_MASK|RemoteKeyboard.ALT_MASK, false);
            keyboardMapper.processAndroidKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, 112));
            keyboardMapper.processAndroidKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, 112));
            rfb.writeKeyEvent(0, savedMetaState, false);
        } else {
            sendKeySym (meta.getKeySym(), meta.getMetaFlags());
        }
    }
}
