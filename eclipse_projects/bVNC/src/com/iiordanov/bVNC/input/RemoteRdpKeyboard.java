package com.iiordanov.bVNC.input;

import android.content.Context;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;

import com.freerdp.freerdpcore.services.LibFreeRDP;
import com.freerdp.freerdpcore.utils.KeyboardMapper;
import com.iiordanov.bVNC.MetaKeyBean;
import com.iiordanov.bVNC.RdpCommunicator;
import com.iiordanov.bVNC.RfbConnectable;
import com.iiordanov.bVNC.VncCanvas;
import com.iiordanov.bVNC.XKeySymCoverter;
import com.iiordanov.tigervnc.rfb.UnicodeToKeysym;

public class RemoteRdpKeyboard implements RemoteKeyboard {
	private final static String TAG = "RemoteKeyboard";
	
	private final static int SCAN_ESC = 1;
	private final static int SCAN_LEFTCTRL = 29;
	private final static int SCAN_RIGHTCTRL = 97;
	private final static int SCAN_F1 = 59;
	private final static int SCAN_F2 = 60;
	private final static int SCAN_F3 = 61;
	private final static int SCAN_F4 = 62;
	private final static int SCAN_F5 = 63;
	private final static int SCAN_F6 = 64;
	private final static int SCAN_F7 = 65;
	private final static int SCAN_F8 = 66;
	private final static int SCAN_F9 = 67;
	private final static int SCAN_F10 = 68;
	//private final static int SCAN_HOME = 102;
	//private final static int SCAN_END = 107;
	
    // Useful shortcuts for modifier masks.
    public final static int CTRL_MASK  = KeyEvent.META_SYM_ON;
    public final static int SHIFT_MASK = KeyEvent.META_SHIFT_ON;
    public final static int ALT_MASK   = KeyEvent.META_ALT_ON;
    public final static int SUPER_MASK = 8;
    public final static int META_MASK  = 0;
	
	private VncCanvas vncCanvas;
	private Handler handler;
	private RfbConnectable rfb;
	private Context context;
	private RdpKeyboardMapper keyboardMapper;

	// Variable holding the state of the on-screen buttons for meta keys (Ctrl, Alt...)
	private int onScreenMetaState = 0;
	// Variable holding the state of any pressed hardware meta keys (Ctrl, Alt...)
	private int hardwareMetaState = 0;
	
	/**
	 * Use camera button as meta key for right mouse button
	 */
	boolean cameraButtonDown = false;
	
	// Keep track when a seeming key press was the result of a menu shortcut
	int lastKeyDown;
	boolean afterMenu;
	
	// Used to convert keysym to keycode
	int deviceID = 0;
	
	/*
	 * Variable used for BB10 hacks.
	 */
	boolean bb10 = false;
	
	
	public RemoteRdpKeyboard (RfbConnectable r, VncCanvas v, Handler h) {
		rfb = r;
		vncCanvas = v;
		handler = h;
		context = v.getContext();
		
		keyboardMapper = new RdpKeyboardMapper();
		keyboardMapper.init(context);
		keyboardMapper.reset((RdpCommunicator)r);
		
		String s = android.os.Build.MODEL;
		if (s.contains("BlackBerry 10"))
			bb10 = true;
	}
	
	
	
	public boolean processLocalKeyEvent(int keyCode, KeyEvent evt) {
		deviceID = evt.getDeviceId();

		if (rfb != null && rfb.isInNormalProtocol()) {
			RemotePointer pointer = vncCanvas.getPointer();
			boolean down = (evt.getAction() == KeyEvent.ACTION_DOWN) ||
						   (evt.getAction() == KeyEvent.ACTION_MULTIPLE);
			
			int metaState = 0, numchars = 1;
			int keyboardMetaState = evt.getMetaState();

		    // Add shift to metaState if necessary.
			// TODO: not interpreting SHIFT for now to avoid sending too many SHIFTs when sending SHIFT+'/' for '?'.
			//if ((keyboardMetaState & KeyEvent.META_SHIFT_MASK) != 0)
			//	metaState |= SHIFT_MASK;
			
			// If the keyboardMetaState contains any hint of CTRL, add CTRL_MASK to metaState
			if ((keyboardMetaState & 0x7000)!=0)
				metaState |= CTRL_MASK;
			// If the keyboardMetaState contains left ALT, add ALT_MASK to metaState.
		    // Leaving KeyEvent.KEYCODE_ALT_LEFT for symbol input on hardware keyboards.
			if ((keyboardMetaState & (KeyEvent.META_ALT_RIGHT_ON|0x00030000)) !=0 )
				metaState |= ALT_MASK;
			
			if (keyCode == KeyEvent.KEYCODE_MENU)
				return true; 			              // Ignore menu key

			if (pointer.handleHardwareButtons(keyCode, evt, metaState|onScreenMetaState|hardwareMetaState))
				return true;

			if (!down) {
				switch (evt.getScanCode()) {
				case SCAN_LEFTCTRL:
				case SCAN_RIGHTCTRL:
					hardwareMetaState &= ~CTRL_MASK;
					break;
				}
				switch(keyCode) {
				case KeyEvent.KEYCODE_DPAD_CENTER:  hardwareMetaState &= ~CTRL_MASK; break;
				// Leaving KeyEvent.KEYCODE_ALT_LEFT for symbol input on hardware keyboards.
				case KeyEvent.KEYCODE_ALT_RIGHT:    hardwareMetaState &= ~ALT_MASK; break;
				}
			} else {
				// Look for standard scan-codes from external keyboards
				switch (evt.getScanCode()) {
				case SCAN_LEFTCTRL:
				case SCAN_RIGHTCTRL:
					hardwareMetaState |= CTRL_MASK;
					break;
				}  
				switch(keyCode) {
				case KeyEvent.KEYCODE_DPAD_CENTER:  hardwareMetaState |= CTRL_MASK; break;
				// Leaving KeyEvent.KEYCODE_ALT_LEFT for symbol input on hardware keyboards.
				case KeyEvent.KEYCODE_ALT_RIGHT:    hardwareMetaState |= ALT_MASK; break;
				}
			}

			android.util.Log.e("RemoteRdpKeyboard", "Sending: " + evt.toString());
			// Update the metaState in RdpCommunicator with writeKeyEvent.
			rfb.writeKeyEvent(keyCode, onScreenMetaState|hardwareMetaState|metaState, down);
			// Send the key to be processed through the KeyboardMapper.
			return keyboardMapper.processAndroidKeyEvent(evt);
		} else {
			return false;
		}
	}
	
	public void sendMetaKey(MetaKeyBean meta) {
		RemotePointer pointer = vncCanvas.getPointer();
		int x = pointer.getX();
		int y = pointer.getY();
		
		if (meta.isMouseClick()) {
			android.util.Log.e("RemoteRdpKeyboard", "is a mouse click");
			int button = meta.getMouseButtons();
			switch (button) {
			case RemoteVncPointer.MOUSE_BUTTON_LEFT:
				pointer.processPointerEvent(x, y, MotionEvent.ACTION_DOWN, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState,
						true, false, false, false, 0);
				break;
			case RemoteVncPointer.MOUSE_BUTTON_RIGHT:
				pointer.processPointerEvent(x, y, MotionEvent.ACTION_DOWN, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState,
						true, true, false, false, 0);
				break;
			case RemoteVncPointer.MOUSE_BUTTON_MIDDLE:
				pointer.processPointerEvent(x, y, MotionEvent.ACTION_DOWN, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState,
						true, false, true, false, 0);
				break;
			case RemoteVncPointer.MOUSE_BUTTON_SCROLL_UP:
				pointer.processPointerEvent(x, y, MotionEvent.ACTION_MOVE, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState,
						true, false, false, true, 0);
				break;
			case RemoteVncPointer.MOUSE_BUTTON_SCROLL_DOWN:
				pointer.processPointerEvent(x, y, MotionEvent.ACTION_MOVE, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState,
						true, false, false, true, 1);
				break;
			}
			try { Thread.sleep(50); } catch (InterruptedException e) {}
			pointer.processPointerEvent(x, y, MotionEvent.ACTION_UP, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState,
					false, false, false, false, 0);

			//rfb.writePointerEvent(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState, button);
			//rfb.writePointerEvent(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState, 0);
		} else {
			char[] s = new char[1];
			s[0] = (char)XKeySymCoverter.keysym2ucs(meta.getKeySym());
			KeyCharacterMap kmap = KeyCharacterMap.load(deviceID);
			KeyEvent events[] = kmap.getEvents(s);

			if (events != null){
				rfb.writeKeyEvent(0, meta.getMetaFlags(), true);
				keyboardMapper.processAndroidKeyEvent(events[0]);
			}
		}
	}
	
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
		// If we find Alt on, turn it off. Otherwise, trurn it on.
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
}
