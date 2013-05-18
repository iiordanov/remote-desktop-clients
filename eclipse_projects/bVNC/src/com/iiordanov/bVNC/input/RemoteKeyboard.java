package com.iiordanov.bVNC.input;

import android.content.Context;
import android.os.Handler;
import android.os.SystemClock;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import com.iiordanov.bVNC.MetaKeyBean;
import com.iiordanov.bVNC.RfbConnectable;
import com.iiordanov.bVNC.VncCanvas;
import com.iiordanov.bVNC.XKeySymCoverter;

public abstract class RemoteKeyboard {
	public final static int SCAN_ESC = 1;
	public final static int SCAN_LEFTCTRL = 29;
	public final static int SCAN_RIGHTCTRL = 97;
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
    public final static int CTRL_MASK  = KeyEvent.META_SYM_ON;
    public final static int SHIFT_MASK = KeyEvent.META_SHIFT_ON;
    public final static int ALT_MASK   = KeyEvent.META_ALT_ON;
    public final static int SUPER_MASK = 8;
    public final static int META_MASK  = 0;
    
	protected VncCanvas vncCanvas;
	protected Handler handler;
	protected RfbConnectable rfb;
	protected Context context;
	protected RdpKeyboardMapper keyboardMapper;

	// Variable holding the state of any pressed hardware meta keys (Ctrl, Alt...)
	protected int hardwareMetaState = 0;
	
	// Use camera button as meta key for right mouse button
	boolean cameraButtonDown = false;
	
	// Keep track when a seeming key press was the result of a menu shortcut
	int lastKeyDown;
	boolean afterMenu;

	// Variable holding the state of the on-screen buttons for meta keys (Ctrl, Alt...)
	protected int onScreenMetaState = 0;
	
	// Variable used for BB10 hacks
	boolean bb10 = false;
	boolean z10 = false;


	public boolean processLocalKeyEvent(int keyCode, KeyEvent evt) { return false; }

	public void sendMetaKey(MetaKeyBean meta) {}
	
	/**
	 * Toggles on-screen Ctrl mask. Returns true if result is Ctrl enabled, false otherwise.
	 * @return true if on false otherwise.
	 */
	public boolean onScreenCtrlToggle()	{
		// If we find Ctrl on, turn it off. Otherwise, turn it on.
		if (onScreenMetaState == (onScreenMetaState | CTRL_MASK)) {
			onScreenMetaState = onScreenMetaState & ~CTRL_MASK;
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
	public void onScreenCtrlOff()	{
		onScreenMetaState = onScreenMetaState & ~CTRL_MASK;
	}
	
	/**
	 * Toggles on-screen Ctrl mask.  Returns true if result is Alt enabled, false otherwise.
	 * @return true if on false otherwise.
	 */
	public boolean onScreenAltToggle() {
		// If we find Alt on, turn it off. Otherwise, turn it on.
		if (onScreenMetaState == (onScreenMetaState | ALT_MASK)) {
			onScreenMetaState = onScreenMetaState & ~ALT_MASK;
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
	public void onScreenAltOff()	{
		onScreenMetaState = onScreenMetaState & ~ALT_MASK;
	}

	public boolean onScreenSuperToggle() {
		// If we find Super on, turn it off. Otherwise, turn it on.
		if (onScreenMetaState == (onScreenMetaState | SUPER_MASK)) {
			onScreenMetaState = onScreenMetaState & ~SUPER_MASK;
			return false;
		}
		else {
			onScreenMetaState = onScreenMetaState | SUPER_MASK;
			return true;
		}
	}
	
	public void onScreenSuperOff() {
		onScreenMetaState = onScreenMetaState & ~SUPER_MASK;		
	}

	public int getMetaState () {
		return onScreenMetaState;
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
					processLocalKeyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
					try { Thread.sleep(10); } catch (InterruptedException e) { }
					processLocalKeyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_UP, keyCode));
				}
			} else {
				event = new KeyEvent(SystemClock.uptimeMillis(), s.substring(i, i+1), KeyCharacterMap.FULL, 0);
				processLocalKeyEvent(event.getKeyCode(), event);
				try { Thread.sleep(10); } catch (InterruptedException e) { }
			}
		}
	}
	
	public void sendKeySym (int keysym, int metaState) {
		char c = (char)XKeySymCoverter.keysym2ucs(keysym);
		sendUnicode(c, metaState);
	}
	
	public void sendUnicode (char unicodeChar, int metaState) {
		KeyCharacterMap kmap = KeyCharacterMap.load(KeyCharacterMap.FULL);
		char[] s = new char[1];
		char[] lowerCaseChar = new char[1];
		s[0] = unicodeChar;
		lowerCaseChar[0] = Character.toLowerCase(unicodeChar);
		KeyEvent[] events = kmap.getEvents(s);
		KeyEvent[] lowerCaseEvts = kmap.getEvents(lowerCaseChar);

		if (events == null) {
			// TODO: Try lower case and convert some symbols.
		} else {
			KeyEvent evt = events[0];
			KeyEvent lowerCaseEvt = lowerCaseEvts[0];
			if (evt.getKeyCode() == KeyEvent.KEYCODE_SHIFT_LEFT || evt.getKeyCode() == KeyEvent.KEYCODE_SHIFT_RIGHT)
				metaState |= RemoteKeyboard.SHIFT_MASK;
			int androidMeta = convertMyMetaToAndroidMeta(metaState);
			KeyEvent down = new KeyEvent(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(), KeyEvent.ACTION_DOWN,
					lowerCaseEvt.getKeyCode(), evt.getRepeatCount(), androidMeta, evt.getDeviceId(), evt.getScanCode());
			android.util.Log.e("RemoteKeyboard", "Sending: " + down.toString());
			processLocalKeyEvent(down.getKeyCode(), down);
			try { Thread.sleep(20); } catch (InterruptedException e) { }
			KeyEvent up = new KeyEvent(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(), KeyEvent.ACTION_UP,
					lowerCaseEvt.getKeyCode(), evt.getRepeatCount(), 0, evt.getDeviceId(), evt.getScanCode());
			android.util.Log.e("RemoteKeyboard", "Sending: " + up.toString());
			processLocalKeyEvent(up.getKeyCode(), up);		
		}
	}
	
	public int convertMyMetaToAndroidMeta (int metaState) {
		int result = 0;
		if ((metaState & RemoteKeyboard.CTRL_MASK) != 0) {
			result |= 0x7000;
		}
		if ((metaState & RemoteKeyboard.ALT_MASK) != 0) {
			android.util.Log.e("", "adding alt mask");
			result |= KeyEvent.META_ALT_RIGHT_ON;
		}
		if ((metaState & RemoteKeyboard.SUPER_MASK) != 0) {
			android.util.Log.e("", "adding alt mask");
			result |= RemoteKeyboard.SUPER_MASK;
		}
		if ((metaState & RemoteKeyboard.SHIFT_MASK) != 0) {
			result |= KeyEvent.META_SHIFT_ON;
		}
		return result;
	}
}
