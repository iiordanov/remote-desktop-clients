package com.keqisoft.android.spice.socket;

import java.lang.Thread.UncaughtExceptionHandler;

import com.freerdp.freerdpcore.services.LibFreeRDP.UIEventListener;

import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

public class Connector {
	
	private ConnectT runThread = null;
	
	static {
		System.loadLibrary("spicec");
	}
	public native int AndroidSpicec(String cmd);
	public native void AndroidSpicecDisconnect();
	public native void AndroidButtonEvent(int x, int y, int metaState, int pointerMask);
	public native void AndroidKeyEvent(boolean keyDown, int virtualKeyCode);
	public native void AndroidSetBitmap(Bitmap newBitmap);

	private static Connector connector = new Connector();
	
	UncaughtExceptionHandler exceptionHandler = new UncaughtExceptionHandler() {
        @Override
        public void uncaughtException(Thread t, Throwable e) {
            synchronized (this) {
                System.err.println("Uncaught exception in thread '" + t.getName() + "': " + e.getMessage());
            }
        }
    };
	
	private Connector() { }
	
	public static Connector getInstance() {
		return connector;
	}

	public static final int CONNECT_SUCCESS = 0;
	public static final int CONNECT_IP_PORT_ERROR = 1;
	public static final int CONNECT_PASSWORD_ERROR = 2;
	public static final int CONNECT_UNKOWN_ERROR = 3;

	private static UIEventListener uiEventListener = null;
	private Handler handler = null;
	private int rs = CONNECT_SUCCESS;

	public void setHandler(Handler handler) {
		this.handler = handler;
	}
	
	public void setUIEventListener(UIEventListener ui) {
		uiEventListener = ui;
	}

	public Handler getHandler() {
		return handler;
	}

	public int connect(String ip, String port, String password) {
		StringBuffer buf = new StringBuffer();
		buf.append("spicy -h ").append(ip);
		buf.append(" -p ").append(port);
		buf.append(" -w ").append(password);
		runThread = new ConnectT(buf.toString());
	    runThread.setUncaughtExceptionHandler(exceptionHandler);
		runThread.start();
		return rs;
	}
	
	public void disconnect() {
		DisconnectT disconnectThread = new DisconnectT();
		disconnectThread.start();
	}

	class ConnectT extends Thread {
		private String cmd;

		public ConnectT(String cmd) {
			this.cmd = cmd;
		}

		public void run() {
			long t1 = System.currentTimeMillis();
			rs = AndroidSpicec(cmd);
			android.util.Log.e("Connector", "Returning from AndroidSpicec.");
			Log.v("Connector", "Connect rs = " + rs + ",cost = " + (System.currentTimeMillis() - t1));

			if (handler != null) {
				handler.sendEmptyMessage(4); /* VncConstants.SPICE_NOTIFICATION */
			}
		}
	}
	
	class DisconnectT extends Thread {
		public DisconnectT() {
		}

		public void run() {
			AndroidSpicecDisconnect();
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
