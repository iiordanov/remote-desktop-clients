/**
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.graphics.drawable.DrawableContainer;
import android.util.Log;

/**
 * @author Michael A. MacDonald
 *
 */
public class AbstractBitmapDrawable extends DrawableContainer {
	Rect cursorRect;
	volatile Rect preCursorRect;
	int hotX, hotY;
	Bitmap softCursor = null;
	Rect clipRect;
	
	AbstractBitmapData data;
	
	static final Paint _defaultPaint;
	static final Paint _whitePaint;
	static final Paint _blackPaint;
	
	static {
		_defaultPaint = new Paint();
		_whitePaint = new Paint();
		_whitePaint.setColor(0xffffffff);
		_blackPaint = new Paint();
		_blackPaint.setColor(0xff000000);
	}

	AbstractBitmapDrawable(AbstractBitmapData data)	{
		this.data = data;
		cursorRect = new Rect();
		preCursorRect = new Rect();
		clipRect = new Rect();
	}
	
	void draw(Canvas canvas, int xoff, int yoff) {
		// If the redrawn area encompasses the rectangle where the cursor was previously,
		// we consider the region cleaned and zero out the rectangle.
		synchronized (preCursorRect) {
			if (canvas.getClipBounds().contains(preCursorRect))
				preCursorRect.setEmpty();
		}

		drawBitmapWithinClip (canvas, xoff, yoff, null);

		if (softCursor != null) {
			canvas.drawBitmap(softCursor, cursorRect.left, cursorRect.top, null);
		}
	}
	
	/**
	 * Draws the bitmap within the current clip.
	 * @param canvas
	 */
	public void drawBitmapWithinClip (Canvas canvas, int xoff, int yoff, Rect toDraw) {
		canvas.drawBitmap(data.mbitmap, xoff, yoff, _defaultPaint);
	}
	
	void setCursorRect(int x, int y, int w, int h, int hX, int hY) {
		hotX = hX;
		hotY = hY;
		cursorRect.left   = x-hotX;
		cursorRect.right  = cursorRect.left + w;
		cursorRect.top    = y-hotY;
		cursorRect.bottom = cursorRect.top + h;
		synchronized (preCursorRect) {
			preCursorRect.union(cursorRect);
		}
	}
	
	void moveCursorRect(int x, int y) {
		setCursorRect(x, y, cursorRect.width(), cursorRect.height(), hotX, hotY);
	}

	void setSoftCursor (int[] newSoftCursorPixels) {
		softCursor = Bitmap.createBitmap(newSoftCursorPixels, cursorRect.width(), cursorRect.height(), Bitmap.Config.ARGB_8888);
	}
	
	/* (non-Javadoc)
	 * @see android.graphics.drawable.DrawableContainer#getIntrinsicHeight()
	 */
	@Override
	public int getIntrinsicHeight() {
		return data.framebufferheight;
	}

	/* (non-Javadoc)
	 * @see android.graphics.drawable.DrawableContainer#getIntrinsicWidth()
	 */
	@Override
	public int getIntrinsicWidth() {
		return data.framebufferwidth;
	}

	/* (non-Javadoc)
	 * @see android.graphics.drawable.DrawableContainer#getOpacity()
	 */
	@Override
	public int getOpacity() {
		return PixelFormat.OPAQUE;
	}

	/* (non-Javadoc)
	 * @see android.graphics.drawable.DrawableContainer#isStateful()
	 */
	@Override
	public boolean isStateful() {
		return false;
	}
}
