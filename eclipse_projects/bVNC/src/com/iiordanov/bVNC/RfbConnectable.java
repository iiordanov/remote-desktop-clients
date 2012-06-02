package com.iiordanov.bVNC;

import java.io.IOException;

public interface RfbConnectable {
	int framebufferWidth ();
	int framebufferHeight ();
	String desktopName ();
	void requestUpdate (boolean incremental) throws IOException;
	void writeClientCutText (String text);
	boolean isInNormalProtocol();
	void writePointerEvent(int x, int y, int metaState, int pointerMask);
	void writeKeyEvent(int key, int metaState, boolean down);
	void processProtocol ();
	void writeSetPixelFormat(int bitsPerPixel, int depth, boolean bigEndian,
			   boolean trueColour, int redMax, int greenMax, int blueMax,
			   int redShift, int greenShift, int blueShift, boolean fGreyScale);
}
