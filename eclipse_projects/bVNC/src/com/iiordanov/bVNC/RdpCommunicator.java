package com.iiordanov.bVNC;

import java.util.Iterator;
import java.util.List;

import android.inputmethodservice.Keyboard;

import com.freerdp.freerdpcore.application.GlobalApp;
import com.freerdp.freerdpcore.application.SessionState;
import com.freerdp.freerdpcore.domain.ManualBookmark;
import com.freerdp.freerdpcore.services.LibFreeRDP;
import com.freerdp.freerdpcore.utils.Mouse;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.bVNC.input.RdpKeyboardMapper;
import com.iiordanov.bVNC.input.RemoteRdpPointer;

public class RdpCommunicator implements RfbConnectable, RdpKeyboardMapper.KeyProcessingListener {
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
	
	SessionState session;
	int metaState = 0;

	boolean isInNormalProtocol = false;

	RdpCommunicator (SessionState session) {
		this.session = session;
	}

	@Override
	public void setIsInNormalProtocol (boolean state) {
		isInNormalProtocol = state;
	}
	
	@Override
	public int framebufferWidth() {
		return session.getBookmark().getActiveScreenSettings().getWidth();
	}

	@Override
	public int framebufferHeight() {
		return session.getBookmark().getActiveScreenSettings().getHeight();
	}

	@Override
	public String desktopName() {
		return ((ManualBookmark)session.getBookmark()).getHostname();
	}

	@Override
	public void requestUpdate(boolean incremental) {
		// NOT USED for RDP.
	}

	@Override
	public void writeClientCutText(String text) {
		// TODO: Needs to be implemented for copy/paste.
	}

	@Override
	public boolean isInNormalProtocol() {
		return isInNormalProtocol;
	}

	@Override
	public String getEncoding() {
		return "RDP";
	}

	@Override
	public void writePointerEvent(int x, int y, int metaState, int pointerMask) {
		this.metaState = metaState;
		if ((pointerMask & RemoteRdpPointer.PTRFLAGS_DOWN) != 0)
			sendModifierKeys(true);
    	LibFreeRDP.sendCursorEvent(session.getInstance(), x, y, pointerMask);
		if ((pointerMask & RemoteRdpPointer.PTRFLAGS_DOWN) == 0)
			sendModifierKeys(false);
	}

	@Override
	public void writeKeyEvent(int key, int metaState, boolean down) {
		// Not used for actually sending keyboard events, but rather to record the current metastate.
		// The key event is sent to the KeyboardMapper from RemoteRdpKeyboard, and
		// when processed through the keyboard mapper, it ends up in one of the KeyProcessingListener
		// methods defined here.
		this.metaState = metaState;
	}

	@Override
	public void writeSetPixelFormat(int bitsPerPixel, int depth,
			boolean bigEndian, boolean trueColour, int redMax, int greenMax,
			int blueMax, int redShift, int greenShift, int blueShift,
			boolean fGreyScale) {
		// NOT USED for RDP.
	}

	@Override
	public void writeFramebufferUpdateRequest(int x, int y, int w, int h,
			boolean b) {
		// NOT USED for RDP.
	}

	@Override
	public void close() {
		LibFreeRDP.disconnect(session.getInstance());
	}
	
	private void sendModifierKeys (boolean down) {
		if ((metaState & RemoteKeyboard.CTRL_MASK) != 0) {
			//android.util.Log.e("RdpCommunicator", "Sending CTRL " + down);
			LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LCONTROL, down);
		}
		if ((metaState & RemoteKeyboard.ALT_MASK) != 0) {
			//android.util.Log.e("RdpCommunicator", "Sending ALT " + down);
			LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LMENU, down);
		}
		if ((metaState & RemoteKeyboard.SUPER_MASK) != 0) {
			//android.util.Log.e("RdpCommunicator", "Sending SUPER " + down);
			LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LWIN | VK_EXT_KEY, down);
		}
		if ((metaState & RemoteKeyboard.SHIFT_MASK) != 0) {
			//android.util.Log.e("RdpCommunicator", "Sending SHIFT " + down);
			LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LSHIFT, down);
		}
	}
	
	// ****************************************************************************
	// KeyboardMapper.KeyProcessingListener implementation
	@Override
	public void processVirtualKey(int virtualKeyCode, boolean down) {
		if (down)
			sendModifierKeys(true);
		//android.util.Log.e("RdpCommunicator", "Sending VK key: " + virtualKeyCode + ". Is it down: " + down);
		LibFreeRDP.sendKeyEvent(session.getInstance(), virtualKeyCode, down);
		if (!down)
			sendModifierKeys(false);
	}

	@Override
	public void processUnicodeKey(int unicodeKey) {
		boolean addShift = false;
		int keyToSend = -1;
		int tempMeta = 0;
		
		// Workarounds for some pesky keys (xrdp needs this for / and ?).
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
		} else {
			//android.util.Log.e("RdpCommunicator", "Sending unicode: " + unicodeKey);
			sendModifierKeys(true);
			LibFreeRDP.sendUnicodeKeyEvent(session.getInstance(), unicodeKey);
			sendModifierKeys(false);
		}
	}

	@Override
	public void switchKeyboard(int keyboardType) {
		// This is functionality specific to aFreeRDP.
	}

	@Override
	public void modifiersChanged() {
		// This is functionality specific to aFreeRDP.
	}

	@Override
	public void requestResolution(int x, int y) {
		// TODO Auto-generated method stub
		
	}
}
