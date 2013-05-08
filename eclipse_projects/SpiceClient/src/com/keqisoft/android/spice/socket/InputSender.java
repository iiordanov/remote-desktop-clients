package com.keqisoft.android.spice.socket;

import java.io.DataOutputStream;
import java.io.IOException;

import android.util.Log;

import com.keqisoft.android.spice.datagram.DGType;
import com.keqisoft.android.spice.datagram.KeyDG;
import com.keqisoft.android.spice.datagram.MouseDG;

public class InputSender {
	private SocketHandler sockHandler = new SocketHandler(
			"/data/data/com.iiordanov.bVNC/aspice-input-socket.socket");

	public void sendKey(KeyDG keyDg) {
		if (!sockHandler.isConnected()) {
			if (!sockHandler.connect()) {
				return;
			}
		}
		//Log.v("keqisoft", "SendKey:" + keyDg.getKeycode());
		try {
			DataOutputStream out = sockHandler.getOut();
			out.writeInt(keyDg.getDgType());
		    out.writeInt(keyDg.getKeycode());
		} catch (IOException e) {
			e.printStackTrace();
			sockHandler.close();
		}
	}

	public void sendMouse(MouseDG mouseDg) {
		if (!sockHandler.isConnected()) {
			if (!sockHandler.connect()) {
				return;
			}
		}
		//Log.v("keqisoft", "SendMouse:x=" + mouseDg.getX() + ",y=" + mouseDg.getY());
		try {
			DataOutputStream out = sockHandler.getOut();
			out.writeInt(mouseDg.getDgType());
			out.writeInt(mouseDg.getButton());
			out.writeInt(mouseDg.getX());
			out.writeInt(mouseDg.getY());
		} catch (IOException e) {
			e.printStackTrace();
			sockHandler.close();
		}
	}

	public void sendOverMsg() {
		if (!sockHandler.isConnected()) {
			if (!sockHandler.connect()) {
				return;
			}
		}
		try {
			DataOutputStream out = sockHandler.getOut();
			out.writeInt(DGType.ANDROID_OVER);
		} catch (IOException e) {
			e.printStackTrace();
			sockHandler.close();
		}
	}

	public void stop() {
		sockHandler.close();
	}
}
