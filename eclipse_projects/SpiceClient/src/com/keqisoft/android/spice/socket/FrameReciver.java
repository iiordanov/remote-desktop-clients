package com.keqisoft.android.spice.socket;

import java.io.DataInputStream;
import java.io.IOException;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapFactory;
import android.graphics.BitmapFactory.Options;
import android.graphics.Canvas;
import android.os.Message;

import com.keqisoft.android.spice.SpiceCanvas;
import com.keqisoft.android.spice.datagram.BitmapDG;

public class FrameReciver {
	private SpiceCanvas canvas;
	private SocketHandler sockHandler = new SocketHandler(
			"/data/data/com.iiordanov.bVNC/aspice-output-socket.socket");
	private boolean keepRecieve = true;
	private FrameRecieveT frameReciveT = null;
	private Options opt = null;

	public FrameReciver(SpiceCanvas canvas) {
		this.canvas = canvas;
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
				BitmapDG bmpDg = canvas.getBitmapDG();
				bmpDg.setDgType(in.readInt());
				bmpDg.setW(in.readInt());
				bmpDg.setH(in.readInt());
				bmpDg.setX(in.readInt());
				bmpDg.setY(in.readInt());
				int size = in.readInt();

				byte[] bs = new byte[size];
				in.readFully(bs);
				Bitmap bmpp = BitmapFactory.decodeByteArray(bs, 0, size, opt);
				bmpDg.setBitmap(combine(bmpp, bmpDg.getY()));

				Message message = new Message();
				message.what = SpiceCanvas.UPDATE_CANVAS;
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
