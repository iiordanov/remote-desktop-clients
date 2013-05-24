package com.iiordanov.bVNC;

import com.freerdp.freerdpcore.services.LibFreeRDP;
import com.iiordanov.bVNC.input.RdpKeyboardMapper;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.bVNC.input.RemoteSpicePointer;
import com.keqisoft.android.spice.socket.Connector;

public class SpiceCommunicator implements RfbConnectable, RdpKeyboardMapper.KeyProcessingListener {
	final static int VK_CONTROL = 0x11;
	final static int VK_LCONTROL = 0xA2;
	final static int VK_RCONTROL = 0xA3;
	final static int VK_LMENU = 0xA4;
	final static int VK_RMENU = 0xA5;
	final static int VK_LSHIFT = 0xA0;
	final static int VK_RSHIFT = 0xA1;
	final static int VK_LWIN = 0x5B;
	final static int VK_RWIN = 0x5C;
	final static int VK_EXT_KEY = 0x00000100;

	int metaState = 0;
	
	private int width = 0;
	private int height = 0;
    
	boolean isInNormalProtocol = false;
	

	public SpiceCommunicator (int w, int h) {
		width = w;
		height = h;
	}
	
	@Override
	public int framebufferWidth() {
		// TODO Auto-generated method stub
		return width;
	}

	@Override
	public int framebufferHeight() {
		// TODO Auto-generated method stub
		return height;
	}

	@Override
	public String desktopName() {
		// TODO Auto-generated method stub
		return "";
	}

	@Override
	public void requestUpdate(boolean incremental) {
		// TODO Auto-generated method stub

	}

	@Override
	public void writeClientCutText(String text) {
		// TODO Auto-generated method stub

	}
	
	@Override
	public void setIsInNormalProtocol(boolean state) {
		isInNormalProtocol = state;		
	}
	
	@Override
	public boolean isInNormalProtocol() {
		return isInNormalProtocol;
	}

	@Override
	public String getEncoding() {
		// TODO Auto-generated method stub
		return "";
	}

	@Override
	public void writePointerEvent(int x, int y, int metaState, int pointerMask) {
		this.metaState = metaState; 
		if ((pointerMask & RemoteSpicePointer.PTRFLAGS_DOWN) != 0)
			sendModifierKeys(true);
		Connector.getInstance().sendMouseEvent(x, y, metaState, pointerMask);
		if ((pointerMask & RemoteSpicePointer.PTRFLAGS_DOWN) == 0)
			sendModifierKeys(false);
	}

	private void sendModifierKeys (boolean keyDown) {		
		if ((metaState & RemoteKeyboard.CTRL_MASK) != 0) {
			android.util.Log.e("SpiceCommunicator", "Sending CTRL: " + VK_LCONTROL);
			Connector.getInstance().sendKeyEvent(keyDown, VK_LCONTROL);
		}
		if ((metaState & RemoteKeyboard.ALT_MASK) != 0) {
			android.util.Log.e("SpiceCommunicator", "Sending ALT: " + VK_LMENU);
			Connector.getInstance().sendKeyEvent(keyDown, VK_LMENU);
		}
		if ((metaState & RemoteKeyboard.SUPER_MASK) != 0) {
			android.util.Log.e("SpiceCommunicator", "Sending SUPER: " + VK_LWIN);
			Connector.getInstance().sendKeyEvent(keyDown, VK_LWIN);
		}
		if ((metaState & RemoteKeyboard.SHIFT_MASK) != 0) {
			android.util.Log.e("SpiceCommunicator", "Sending SHIFT: " + VK_LSHIFT);
			Connector.getInstance().sendKeyEvent(keyDown, VK_LSHIFT);
		}
	}
	
	@Override
	public void writeKeyEvent(int key, int metaState, boolean down) {
		// Not used for actually sending keyboard events, but rather to record the current metastate.
		// The key event is sent to the KeyboardMapper from RemoteSpiceKeyboard, and
		// when processed through the keyboard mapper, it ends up in one of the KeyProcessingListener
		// methods defined here.
		this.metaState = metaState;
	}

	@Override
	public void writeSetPixelFormat(int bitsPerPixel, int depth,
			boolean bigEndian, boolean trueColour, int redMax, int greenMax,
			int blueMax, int redShift, int greenShift, int blueShift,
			boolean fGreyScale) {
		// TODO Auto-generated method stub

	}

	@Override
	public void writeFramebufferUpdateRequest(int x, int y, int w, int h,
			boolean b) {
		// TODO Auto-generated method stub

	}

	@Override
	public void close() {
		Connector.getInstance().disconnect();
	}
	
	// ****************************************************************************
	// KeyboardMapper.KeyProcessingListener implementation
	@Override
	public void processVirtualKey(int virtualKeyCode, boolean keyDown) {

		if (keyDown)
			sendModifierKeys (true);
		
		//android.util.Log.e("SpiceCommunicator", "Sending VK key: " + virtualKeyCode + ". Is it down: " + down);
		Connector.getInstance().sendKeyEvent(keyDown, virtualKeyCode);
		
		if (!keyDown)
			sendModifierKeys (false);
		
	}

	@Override
	public void processUnicodeKey(int unicodeKey) {
		boolean addShift = false;
		int keyToSend = -1;
		int tempMeta = 0;
		
		// Workarounds for some pesky keys.
		if (unicodeKey == 64) {
			addShift = true;
			keyToSend = 0x32;
		} else if (unicodeKey == 47) {
			keyToSend = 0xBF;
		} else if (unicodeKey == 63) {
			addShift = true;			
			keyToSend = 0xBF;
		}
		
		if (keyToSend != -1) {
			tempMeta = metaState;
			if (addShift) {
				metaState = metaState |  RemoteKeyboard.SHIFT_MASK;
			}
			processVirtualKey(keyToSend, true);
			processVirtualKey(keyToSend, false);
			metaState = tempMeta;
		} else
			android.util.Log.e("SpiceCommunicator", "Unsupported unicode key that needs to be mapped: " + unicodeKey);
	}

	@Override
	public void switchKeyboard(int keyboardType) {
		// This is functionality specific to aFreeRDP.
	}

	@Override
	public void modifiersChanged() {
		// This is functionality specific to aFreeRDP.
	}
}
