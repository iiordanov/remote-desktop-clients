package com.keqisoft.android.spice.socket;

import java.io.DataInputStream;
import java.io.IOException;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapFactory;
import android.graphics.BitmapFactory.Options;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Message;

import com.keqisoft.android.spice.SpiceCanvas;
import com.keqisoft.android.spice.datagram.BitmapDG;

public class FrameReciver2 {
	private Bitmap bitmap;
	private Canvas canvas;
	private SocketHandler sockHandler = new SocketHandler(
			"/data/data/com.iiordanov.bVNC/aspice-output-socket.socket");
	private boolean keepRecieve = true;
	private FrameRecieveT frameReciveT = null;
	private Options opt = null;

	public FrameReciver2(Bitmap bitmap) {
		this.bitmap = bitmap;
		canvas = new Canvas(this.bitmap);
		opt = new Options();
		opt.inPreferredConfig = Config.RGB_565;
	}

	public void startRecieveFrame() {
		if (frameReciveT == null) {
			frameReciveT = new FrameRecieveT();
			frameReciveT.start();
		}
	}

	public void stop() {
		keepRecieve = false;
//		closeFile();
		sockHandler.close();
	}

	class FrameRecieveT extends Thread {
		public void run() {
			while (keepRecieve) {
				recive();
			}
		}
	}

	private void recive() {
		if (sockHandler.isConnected()) {
			try {
				DataInputStream in = sockHandler.getInput();
				int dgtype = in.readInt();
				int w = in.readInt();
				int h = in.readInt();
				int x = in.readInt();
				int y = in.readInt();
				int size = in.readInt();
				byte[] bs = new byte[size];
				in.readFully(bs);
				Bitmap bmpp = BitmapFactory.decodeByteArray(bs, 0, size, opt);
				Rect d = new Rect(x, y, x+w, y+h);
				canvas.drawBitmap(bmpp, null, d, null);
				//android.util.Log.e("", "RECEIVED: x: " + x + " y: " +y+ " w: " +w+ " h: " +h + " size: " + size);

				Message message = new Message();
				message.what = SpiceCanvas.UPDATE_CANVAS;
				message.obj = d;
				Connector.getInstance().getHandler().sendMessage(message);
			} catch (IOException e) {
				sockHandler.close();
			}
		} else {
			if (!sockHandler.connect()) {
				try {
					Thread.sleep(1000);
				} catch (InterruptedException e) {
				}
			}
		}
	}

	private Canvas cvs = null;
	private Bitmap bmpOverlay = null;
	private Bitmap combine(Bitmap bmp, int y) {
		if (bmpOverlay == null) {
			bmpOverlay = Bitmap.createBitmap(bmp.getWidth(), bmp.getHeight(), bmp.getConfig());
			cvs = new Canvas(bmpOverlay);
			cvs.drawBitmap(bmp, 0, 0, null);
			return bmp;
		}
		cvs.drawBitmap(bmpOverlay, 0, 0, null);
		cvs.drawBitmap(bmp, 0, y, null);
		return bmpOverlay;
	}
}
