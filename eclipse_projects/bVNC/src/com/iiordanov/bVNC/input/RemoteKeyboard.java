package com.iiordanov.bVNC.input;

import android.os.Handler;
import android.util.Log;
import android.view.KeyEvent;

import com.iiordanov.bVNC.MetaKeyBean;
import com.iiordanov.bVNC.RfbConnectable;
import com.iiordanov.bVNC.VncCanvas;
import com.iiordanov.tigervnc.rfb.UnicodeToKeysym;

public class RemoteKeyboard {
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
    public final static int META_MASK  = 0;
	
	private VncCanvas vncCanvas;
	private Handler handler;
	private RfbConnectable rfb;

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
	
	/*
	 * Variable used for BB10 hacks.
	 */
	boolean bb10 = false;
	
	
	public RemoteKeyboard (RfbConnectable r, VncCanvas v, Handler h) {
		rfb = r;
		vncCanvas = v;
		handler = h;
		
		String s = android.os.Build.MODEL;
		if (s.contains("BlackBerry 10"))
			bb10 = true;
	}
	
	
	
	public boolean processLocalKeyEvent(int keyCode, KeyEvent evt) {
		if (rfb != null && rfb.isInNormalProtocol()) {
			RemotePointer pointer = vncCanvas.getPointer();
			boolean down = (evt.getAction() == KeyEvent.ACTION_DOWN) ||
						   (evt.getAction() == KeyEvent.ACTION_MULTIPLE);
			
			int metaState = 0, numchars = 1;
			int keyboardMetaState = evt.getMetaState();

		    // Add shift to metaState if necessary.
			if ((keyboardMetaState & KeyEvent.META_SHIFT_MASK) != 0)
				metaState |= SHIFT_MASK;
			
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

		   int key = 0, keysym = 0;
		   
		   if (!down) {
			   switch (evt.getScanCode()) {
			   case SCAN_ESC:               key = 0xff1b; break;
			   case SCAN_LEFTCTRL:
			   case SCAN_RIGHTCTRL:
				   hardwareMetaState &= ~CTRL_MASK;
				   break;
			   case SCAN_F1:				keysym = 0xffbe;				break;
			   case SCAN_F2:				keysym = 0xffbf;				break;
			   case SCAN_F3:				keysym = 0xffc0;				break;
			   case SCAN_F4:				keysym = 0xffc1;				break;
			   case SCAN_F5:				keysym = 0xffc2;				break;
			   case SCAN_F6:				keysym = 0xffc3;				break;
			   case SCAN_F7:				keysym = 0xffc4;				break;
			   case SCAN_F8:				keysym = 0xffc5;				break;
			   case SCAN_F9:				keysym = 0xffc6;				break;
			   case SCAN_F10:				keysym = 0xffc7;				break;
			   }

			   switch(keyCode) {
			      case KeyEvent.KEYCODE_DPAD_CENTER:  hardwareMetaState &= ~CTRL_MASK; break;
			      // Leaving KeyEvent.KEYCODE_ALT_LEFT for symbol input on hardware keyboards.
			   	  case KeyEvent.KEYCODE_ALT_RIGHT:    hardwareMetaState &= ~ALT_MASK; break;
			   }
		   }
	   
		   switch(keyCode) {
		   	  case KeyEvent.KEYCODE_BACK:         keysym = 0xff1b; break;
		      case KeyEvent.KEYCODE_DPAD_LEFT:    keysym = 0xff51; break;
		   	  case KeyEvent.KEYCODE_DPAD_UP:      keysym = 0xff52; break;
		   	  case KeyEvent.KEYCODE_DPAD_RIGHT:   keysym = 0xff53; break;
		   	  case KeyEvent.KEYCODE_DPAD_DOWN:    keysym = 0xff54; break;
		      case KeyEvent.KEYCODE_DEL: 		  keysym = 0xff08; break;
		      case KeyEvent.KEYCODE_ENTER:        keysym = 0xff0d; break;
		   	  case KeyEvent.KEYCODE_TAB:          keysym = 0xff09; break;
		   	  case 92 /* KEYCODE_PAGE_UP */:      keysym = 0xff55; break;
		   	  case 93 /* KEYCODE_PAGE_DOWN */:    keysym = 0xff56; break;
		   	  case 111 /* KEYCODE_ESCAPE */:      keysym = 0xff1b; break;
		   	  case 112 /* KEYCODE_FORWARD_DEL */: keysym = 0xffff; break;
		   	  case 113 /* KEYCODE_CTRL_LEFT */:   keysym = 0xffe3; break;
		   	  case 114 /* KEYCODE_CTRL_RIGHT */:  keysym = 0xffe4; break;
		   	  case 115 /* KEYCODE_CAPS_LOCK */:   keysym = 0xffe5; break;
		   	  case 116 /* KEYCODE_SCROLL_LOCK */: keysym = 0xff14; break;
		   	  case 120 /* KEYCODE_SYSRQ */:       keysym = 0xff61; break;
		   	  case 121 /* KEYCODE_BREAK */:       keysym = 0xff6b; break;
		   	  case 122 /* KEYCODE_MOVE_HOME */:   keysym = 0xff50; break;
		   	  case 123 /* KEYCODE_MOVE_END */:    keysym = 0xff57; break;
		   	  case 124 /* KEYCODE_INSERT */:      keysym = 0xff63; break;
		   	  case 131 /* KEYCODE_F1 */:          keysym = 0xffbe; break;
		   	  case 132 /* KEYCODE_F2 */:          keysym = 0xffbf; break;
		   	  case 133 /* KEYCODE_F3 */:          keysym = 0xffc0; break;
		   	  case 134 /* KEYCODE_F4 */:          keysym = 0xffc1; break;
		   	  case 135 /* KEYCODE_F5 */:          keysym = 0xffc2; break;
		   	  case 136 /* KEYCODE_F6 */:          keysym = 0xffc3; break;
		   	  case 137 /* KEYCODE_F7 */:          keysym = 0xffc4; break;
		   	  case 138 /* KEYCODE_F8 */:          keysym = 0xffc5; break;
		   	  case 139 /* KEYCODE_F9 */:          keysym = 0xffc6; break;
		   	  case 140 /* KEYCODE_F10 */:         keysym = 0xffc7; break;
		   	  case 141 /* KEYCODE_F11 */:         keysym = 0xffc8; break;
		   	  case 142 /* KEYCODE_F12 */:         keysym = 0xffc9; break;
		   	  case 143 /* KEYCODE_NUM_LOCK */:    keysym = 0xff7f; break;
		   	  case 0   /* KEYCODE_UNKNOWN */:
		   		  if (evt.getCharacters() != null) {
		   			  key = evt.getCharacters().charAt(0);
		   			  keysym = UnicodeToKeysym.translate(key);
		   			  numchars = evt.getCharacters().length();
		   		  }
	    		  break;
		      default: 							  
		    	  // Modifier handling is a bit tricky. Alt and Ctrl should be passed
		    	  // through to the VNC server so that they get handled there, but strip
		    	  // them from the character before retrieving the Unicode char from it.
		    	  // Don't clear Shift, we still want uppercase characters.
		    	  int vncEventMask = ( 0x7000 );   // KeyEvent.META_CTRL_MASK
		    	  if ((metaState & ALT_MASK) != 0)
		    		  vncEventMask |= 0x0032;      // KeyEvent.META_ALT_MASK
		    	  KeyEvent copy = new KeyEvent(evt.getDownTime(), evt.getEventTime(), evt.getAction(),
		    			  						evt.getKeyCode(),  evt.getRepeatCount(),
		    				  					keyboardMetaState & ~vncEventMask, evt.getDeviceId(), evt.getScanCode());
		    	  key = copy.getUnicodeChar();
	    		  keysym = UnicodeToKeysym.translate(key);
		    	  break;
		   }

		   if (down) {
			   // Look for standard scan-codes from external keyboards
			   switch (evt.getScanCode()) {
			   case SCAN_ESC:               keysym = 0xff1b; break;
			   case SCAN_LEFTCTRL:
			   case SCAN_RIGHTCTRL:
				   hardwareMetaState |= CTRL_MASK;
				   break;
			   case SCAN_F1:				keysym = 0xffbe;				break;
			   case SCAN_F2:				keysym = 0xffbf;				break;
			   case SCAN_F3:				keysym = 0xffc0;				break;
			   case SCAN_F4:				keysym = 0xffc1;				break;
			   case SCAN_F5:				keysym = 0xffc2;				break;
			   case SCAN_F6:				keysym = 0xffc3;				break;
			   case SCAN_F7:				keysym = 0xffc4;				break;
			   case SCAN_F8:				keysym = 0xffc5;				break;
			   case SCAN_F9:				keysym = 0xffc6;				break;
			   case SCAN_F10:				keysym = 0xffc7;				break;
			   }
			   
			   switch(keyCode) {
			      case KeyEvent.KEYCODE_DPAD_CENTER:  hardwareMetaState |= CTRL_MASK; break;
			      // Leaving KeyEvent.KEYCODE_ALT_LEFT for symbol input on hardware keyboards.
			   	  case KeyEvent.KEYCODE_ALT_RIGHT:    hardwareMetaState |= ALT_MASK; break;
			   }
		   }

		   try {
			   if (afterMenu) {
				   afterMenu = false;
				   if (!down && keysym != lastKeyDown)
					   return true;
			   }
			   if (down)
				   lastKeyDown = keysym;

			   if (numchars == 1) {
			       //Log.e(TAG,"action down? = " + down + " key = " + key + " keysym = " + keysym + " onscreen metastate = " + onScreenMetaState + " keyboard metastate = " + keyboardMetaState + " RFB metastate = " + metaState + " keycode = " + keyCode + " unicode = " + evt.getUnicodeChar());
				   rfb.writeKeyEvent(keysym, (onScreenMetaState|hardwareMetaState|metaState), down);

				   // TODO: UGLY HACK for BB10 devices which never send the up-event
				   // for space, backspace and enter... so we send it instead. Remove as soon as possible!
				   if (bb10 && (keyCode == KeyEvent.KEYCODE_SPACE || keyCode == KeyEvent.KEYCODE_DEL ||
						   		keyCode == KeyEvent.KEYCODE_ENTER))
					   rfb.writeKeyEvent(keysym, (onScreenMetaState|hardwareMetaState|metaState), false);

			   } else if (numchars > 1) {
				   for (int i = 0; i < numchars; i++) {
					   key = evt.getCharacters().charAt(i);
					   //Log.e(TAG,"action down? = " + down + " key = " + key + " keysym = " + keysym + " onscreen metastate = " + onScreenMetaState + " keyboard metastate = " + keyboardMetaState + " RFB metastate = " + metaState + " keycode = " + keyCode + " unicode = " + evt.getUnicodeChar());
					   keysym = UnicodeToKeysym.translate(key);
					   rfb.writeKeyEvent(keysym, (onScreenMetaState|hardwareMetaState|metaState), down);
				   }
			   }
		   } catch (Exception e) {
			   e.printStackTrace();
		   }
		   return true;
		}
		return false;
	}
	
	public void sendMetaKey(MetaKeyBean meta) {
		RemotePointer pointer = vncCanvas.getPointer();
		int x = pointer.getX();
		int y = pointer.getY();
		
		if (meta.isMouseClick())
		{
			rfb.writePointerEvent(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState, meta.getMouseButtons());
			rfb.writePointerEvent(x, y, meta.getMetaFlags()|onScreenMetaState|hardwareMetaState, 0);
		}
		else {
			rfb.writeKeyEvent(meta.getKeySym(), meta.getMetaFlags()|onScreenMetaState|hardwareMetaState, true);
			rfb.writeKeyEvent(meta.getKeySym(), meta.getMetaFlags()|onScreenMetaState|hardwareMetaState, false);
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
