package com.keqisoft.android.spice.socket;

import java.lang.Thread.UncaughtExceptionHandler;

import com.freerdp.freerdpcore.services.LibFreeRDP.UIEventListener;

import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

public class Connector {

	public native int AndroidSpicec(String ip, String port, String password);
	public native void AndroidSpicecDisconnect();
	public native void AndroidButtonEvent(int x, int y, int metaState, int pointerMask);
	public native void AndroidKeyEvent(boolean keyDown, int virtualKeyCode);
	public native void AndroidSetBitmap(Bitmap newBitmap);
	
	private ConnectT runThread = null;
	
	static {
		System.loadLibrary("spicec");
	}

	private static Connector connector = new Connector();
	
	private Connector() { }
	
	public static Connector getInstance() {
		return connector;
	}

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

	public void connect(String ip, String port, String password) {
		runThread = new ConnectT(ip, port, password);
		runThread.start();
	}
	
	public void disconnect() {
		AndroidSpicecDisconnect();
		try {runThread.join(3000);} catch (InterruptedException e) {}
	}

	class ConnectT extends Thread {
		private String ip, port, password;

		public ConnectT(String ip, String port, String password) {
			this.ip = ip;
			this.port = port;
			this.password = password;
		}

		public void run() {
			AndroidSpicec(ip, port, password);
			android.util.Log.e("Connector", "Returning from AndroidSpicec.");

			if (handler != null) {
				handler.sendEmptyMessage(4); /* VncConstants.SPICE_NOTIFICATION */
			}
		}
	}
	
	public void sendMouseEvent (int x, int y, int metaState, int pointerMask) {
		AndroidButtonEvent(x, y, metaState, pointerMask);
	}

	public void sendKeyEvent (boolean keyDown, int virtualKeyCode) {
		AndroidKeyEvent(keyDown, virtualKeyCode);
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
	
	
}
