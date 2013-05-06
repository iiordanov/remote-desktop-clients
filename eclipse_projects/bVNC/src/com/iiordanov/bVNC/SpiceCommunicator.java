package com.iiordanov.bVNC;

import com.keqisoft.android.spice.socket.FrameReceiver;

public class SpiceCommunicator implements RfbConnectable {

	private int width = 0;
	private int height = 0;
	private FrameReceiver frameReceiver = null;
	
	public SpiceCommunicator (FrameReceiver f, int w, int h) {
		width = w;
		height = h;
		frameReceiver = f;
	}
	
	@Override
	public int framebufferWidth() {
		// TODO Auto-generated method stub
		return width;
	}

	@Override
	public int framebufferHeight() {
		// TODO Auto-generated method stub
		return height;
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
	public boolean isInNormalProtocol() {
		// TODO Auto-generated method stub
		return true;
	}

	@Override
	public String getEncoding() {
		// TODO Auto-generated method stub
		return "";
	}

	@Override
	public void writePointerEvent(int x, int y, int metaState, int pointerMask) {
		// TODO Auto-generated method stub

	}

	@Override
	public void writeKeyEvent(int key, int metaState, boolean down) {
		// TODO Auto-generated method stub

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
		frameReceiver.stop();
	}

}
