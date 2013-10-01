package com.iiordanov.bVNC;

import android.graphics.Bitmap;
import android.os.Handler;

import com.freerdp.freerdpcore.services.LibFreeRDP.UIEventListener;
import com.iiordanov.bVNC.input.RdpKeyboardMapper;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.bVNC.input.RemoteSpicePointer;

public class SpiceCommunicator implements RfbConnectable, RdpKeyboardMapper.KeyProcessingListener {
	private final static String TAG = "SpiceCommunicator";

	public native int  SpiceClientConnect (String ip, String port, String tport, String password, String ca_file, String cert_subj);
	public native void SpiceClientDisconnect ();
	public native void SpiceButtonEvent (int x, int y, int metaState, int pointerMask);
	public native void SpiceKeyEvent (boolean keyDown, int virtualKeyCode);
	public native void UpdateBitmap (Bitmap bitmap, int x, int y, int w, int h);
	public native void SpiceRequestResolution (int x, int y);
	/*
	static {
		System.loadLibrary("gstreamer_android");
		System.loadLibrary("spice");
	}
	*/
	
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
	
	private SpiceThread spicehread = null;

	public SpiceCommunicator () { }

	private static UIEventListener uiEventListener = null;
	private Handler handler = null;

	public void setHandler(Handler handler) {
		this.handler = handler;
	}
	
	public void setUIEventListener(UIEventListener ui) {
		uiEventListener = ui;
	}

	public Handler getHandler() {
		return handler;
	}

	public void connect(String ip, String port, String tport, String password, String cf, String cs) {
		android.util.Log.e(TAG, ip + ", " + port + ", " + tport + ", " + password + ", " + cf + ", " + cs);
		spicehread = new SpiceThread(ip, port, tport, password, cf, cs);
		spicehread.start();
	}
	
	public void disconnect() {
		SpiceClientDisconnect();
		try {spicehread.join(3000);} catch (InterruptedException e) {}
	}

	class SpiceThread extends Thread {
		private String ip, port, tport, password, cf, cs;

		public SpiceThread(String ip, String port, String tport, String password, String cf, String cs) {
			this.ip = ip;
			this.port = port;
			this.tport = tport;
			this.password = password;
			this.cf = cf;
			this.cs = cs;
		}

		public void run() {
			SpiceClientConnect (ip, port, tport, password, cf, cs);
			android.util.Log.e(TAG, "SpiceClientConnect returned.");

			// If we've exited SpiceClientConnect, the connection is certainly
			// interrupted or was never established.
			if (handler != null) {
				handler.sendEmptyMessage(VncConstants.SPICE_CONNECT_FAILURE);
			}
		}
	}
	
	public void sendMouseEvent (int x, int y, int metaState, int pointerMask) {
		SpiceButtonEvent(x, y, metaState, pointerMask);
	}

	public void sendKeyEvent (boolean keyDown, int virtualKeyCode) {
		SpiceKeyEvent(keyDown, virtualKeyCode);
	}
	
	
	/* Callbacks from jni */
	private static void OnSettingsChanged(int inst, int width, int height, int bpp) {
		if (uiEventListener != null)
			uiEventListener.OnSettingsChanged(width, height, bpp);
	}

	private static boolean OnAuthenticate(int inst, StringBuilder username, StringBuilder domain, StringBuilder password) {
		if (uiEventListener != null)
			return uiEventListener.OnAuthenticate(username, domain, password);
		return false;
	}

	private static boolean OnVerifyCertificate(int inst, String subject, String issuer, String fingerprint) {
		if (uiEventListener != null)
			return uiEventListener.OnVerifiyCertificate(subject, issuer, fingerprint);
		return false;
	}

	private static void OnGraphicsUpdate(int inst, int x, int y, int width, int height) {
		if (uiEventListener != null)
			uiEventListener.OnGraphicsUpdate(x, y, width, height);
	}

	private static void OnGraphicsResize(int inst, int width, int height, int bpp) {
		android.util.Log.e("Connector", "onGraphicsResize, width: " + width + " height: " + height);
		if (uiEventListener != null)
			uiEventListener.OnGraphicsResize(width, height, bpp);
	}
	
	@Override
	public int framebufferWidth() {
		return width;
	}

	@Override
	public int framebufferHeight() {
		return height;
	}

	public void setFramebufferWidth(int w) {
		width = w;
	}

	public void setFramebufferHeight(int h) {
		height = h;
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
		sendMouseEvent(x, y, metaState, pointerMask);
		if ((pointerMask & RemoteSpicePointer.PTRFLAGS_DOWN) == 0)
			sendModifierKeys(false);
	}

	private void sendModifierKeys (boolean keyDown) {		
		if ((metaState & RemoteKeyboard.CTRL_MASK) != 0) {
			//android.util.Log.e("SpiceCommunicator", "Sending CTRL: " + VK_LCONTROL);
			sendKeyEvent(keyDown, VK_LCONTROL);
		}
		if ((metaState & RemoteKeyboard.ALT_MASK) != 0) {
			//android.util.Log.e("SpiceCommunicator", "Sending ALT: " + VK_LMENU);
			sendKeyEvent(keyDown, VK_LMENU);
		}
		if ((metaState & RemoteKeyboard.SUPER_MASK) != 0) {
			//android.util.Log.e("SpiceCommunicator", "Sending SUPER: " + VK_LWIN);
			sendKeyEvent(keyDown, VK_LWIN);
		}
		if ((metaState & RemoteKeyboard.SHIFT_MASK) != 0) {
			//android.util.Log.e("SpiceCommunicator", "Sending SHIFT: " + VK_LSHIFT);
			sendKeyEvent(keyDown, VK_LSHIFT);
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
		disconnect();
	}
	
	// ****************************************************************************
	// KeyboardMapper.KeyProcessingListener implementation
	@Override
	public void processVirtualKey(int virtualKeyCode, boolean keyDown) {

		if (keyDown)
			sendModifierKeys (true);
		
		//android.util.Log.e("SpiceCommunicator", "Sending VK key: " + virtualKeyCode + ". Is it down: " + down);
		sendKeyEvent(keyDown, virtualKeyCode);
		
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
	
	@Override
	public void requestResolution(int x, int y) {
		SpiceRequestResolution (x, y);		
	}
}
