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
	public native int AndroidSpicecDisconnect();

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
		// 连接如果成功，ConnectT线程会一直阻塞。如果连接失败了，线程里的方法会迅速返回，最多等待3秒后取结果
//		try {
//			Thread.sleep(5000);
//		} catch (InterruptedException e) {
//		}
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
}
