package com.iiordanov.bVNC.input;

import com.iiordanov.bVNC.RfbConnectable;

import android.os.Handler;
import android.view.KeyEvent;

public class KeyRepeater implements Runnable {

	private RemoteKeyboard keyboard = null;
	private Handler handler = null;;
	private int keyCode = 0;
	private KeyEvent event = null;
	private int interval = 150;
	
	public KeyRepeater (RemoteKeyboard keyboard, Handler handler) {
		this.keyboard = keyboard;
		this.handler = handler;
	}
	
	public void start (int keyCode, KeyEvent event) {
		stop();
		this.keyCode = keyCode;
		this.event = event;
		handler.post(this);
	}
	
	public void stop () {
		handler.removeCallbacks(this);
	}
	
	@Override
	public void run() {
		keyboard.processLocalKeyEvent(keyCode, event);
		handler.postDelayed(this, interval);
	}

}
