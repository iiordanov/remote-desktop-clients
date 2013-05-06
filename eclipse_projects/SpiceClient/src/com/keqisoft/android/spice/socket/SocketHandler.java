package com.keqisoft.android.spice.socket;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.net.LocalSocketAddress.Namespace;
import android.util.Log;

public class SocketHandler {
	private LocalSocket socket = null;
	private DataInputStream in = null;
	private DataOutputStream out = null;
	private String filePath = null;

	public SocketHandler(String filePath) {
		this.filePath = filePath;
	}
	
	public boolean isConnected() {
		return socket != null && socket.isConnected();
	}

	/**
	 * 建立连接
	 * 
	 * @return
	 */
	public boolean connect() {
		try {
			Log.v("keqisoft","Try to connec unix socket " + filePath);
			socket = new LocalSocket();
			socket.connect(new LocalSocketAddress(filePath,Namespace.FILESYSTEM));
			Log.v("keqisoft","Unix socket " + filePath + " connected ...");
			return true;
		} catch (IOException e) {
			close();
		}
		return false;
	}
	
	public DataInputStream getInput() throws IOException {
		in = new DataInputStream(socket.getInputStream());
		return in;
	}
	
	public DataOutputStream getOut() throws IOException {
		out = new DataOutputStream(socket.getOutputStream());
		return out;
	}

	/**
	 * 关闭连接
	 */
	public void close() {
		if (in != null) {
			try {
				in.close();
			} catch (IOException e) {
			}
		}
		if (out != null) {
			try {
				out.close();
			} catch (IOException e) {
			}
		}
		if (socket != null) {
			try {
				socket.close();
			} catch (IOException e) {
			}
		}
	}
}
