package com.keqisoft.android.spice.socket;

import android.os.Handler;
import android.os.Message;
import android.util.Log;

public class Connector {
	
	private ConnectT runThread = null;
	
	static {
		System.loadLibrary("spicec");// load libspicec.so
	}
	public native int AndroidSpicec(String cmd);
	public native void AndroidSpicecDisconnect();
	public native void AndroidButtonEvent(int x, int y, int metaState, int type);
	public native void AndroidKeyEvent(int keyDown, int virtualKeyCode);


	// 单例
	private static Connector connector = new Connector();
	private Connector() {
	}
	public static Connector getInstance() {
		return connector;
	}

	public static final int CONNECT_SUCCESS = 0;
	public static final int CONNECT_IP_PORT_ERROR = 1;
	public static final int CONNECT_PASSWORD_ERROR = 2;
	public static final int CONNECT_UNKOWN_ERROR = 3;

	private Handler handler = null;
	private int rs = CONNECT_SUCCESS;

	public void setHandler(Handler handler) {
		this.handler = handler;
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
				Message message = new Message();
				message.what = rs;
				handler.sendMessage(message);
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
	
	public void sendMouseEvent (int x, int y, int metaState, int type) {
		AndroidButtonEvent(x, y, metaState, type);
	}

	public void sendKeyEvent (int keyDown, int virtualKeyCode) {
		AndroidKeyEvent(keyDown, virtualKeyCode);
	}
}
