package com.iiordanov.bVNC;

import java.io.IOException;

public interface RfbConnectable {
	int framebufferWidth ();
	int framebufferHeight ();
	String desktopName ();
	void requestUpdate (boolean incremental);
	void writeClientCutText (String text);
	boolean isInNormalProtocol();
	String getEncoding ();
	void writePointerEvent(int x, int y, int metaState, int pointerMask);
	void writeKeyEvent(int key, int metaState, boolean down);
	void writeSetPixelFormat(int bitsPerPixel, int depth, boolean bigEndian,
			   boolean trueColour, int redMax, int greenMax, int blueMax,
			   int redShift, int greenShift, int blueShift, boolean fGreyScale);
	void writeFramebufferUpdateRequest(int x, int y, int w, int h,	boolean b);
	void close();
}
