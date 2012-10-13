/**
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.DrawableContainer;

/**
 * @author Michael A. MacDonald
 *
 */
public class AbstractBitmapDrawable extends DrawableContainer {
	Rect cursorRect;
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
		clipRect = new Rect();
	}
	
	void draw(Canvas canvas, int xoff, int yoff) {
		canvas.drawBitmap(data.mbitmap, xoff, yoff, _defaultPaint);
		setCursorRectAndDrawIfNecessary(canvas);
	}
	
	/**
	 * Sets the local cursor Rect, the clip which contains it, and draws it if necessary.
	 * @param canvas
	 */
	void setCursorRectAndDrawIfNecessary (Canvas canvas) {
		if(data.vncCanvas.connection.getUseLocalCursor()) {
			setCursorRect(data.vncCanvas.mouseX, data.vncCanvas.mouseY);
			Rect toClip = new Rect (cursorRect.left, cursorRect.top,
					cursorRect.left + 4*cursorRect.width(), cursorRect.top + 5*cursorRect.height());
			clipRect.set(toClip);
			if (canvas.clipRect(toClip)) {
				drawCursor(canvas);
			}
		}
	}
	
	/**
	 * Draws an easily visible local pointer made of increasingly larger rectangles.
	 * @param canvas
	 */
	void drawCursor(Canvas canvas) {
		int x = cursorRect.left;
		int y = cursorRect.top;
		int h = cursorRect.height();
		int w = cursorRect.width();
		Rect one   = new Rect (x, y+h, x+2*w, y+2*h);
		Rect two   = new Rect (x, y+2*h, x+3*w, y+3*h);
		Rect three = new Rect (x, y+3*h, x+4*w, y+5*h);
		
		canvas.drawRect(cursorRect,_blackPaint);
		canvas.drawRect(one,       _blackPaint);
		canvas.drawRect(two,       _blackPaint);
		canvas.drawRect(three,     _blackPaint);
		canvas.drawRect((float)cursorRect.left + 1,  (float)cursorRect.top + 1,
						(float)cursorRect.right - 1, (float)cursorRect.bottom + 1, _whitePaint);
		canvas.drawRect((float)one.left + 1,    (float)one.top + 1,
						(float)one.right - 1,   (float)one.bottom + 1, _whitePaint);
		canvas.drawRect((float)two.left + 1,    (float)two.top + 1,
						(float)two.right - 1,   (float)two.bottom + 1, _whitePaint);
		canvas.drawRect((float)three.left + 1,  (float)three.top + 1,
						(float)three.right - 1, (float)three.bottom - 1, _whitePaint);
	}
	
	void setCursorRect(int mouseX, int mouseY) {
		cursorRect.left = mouseX - 2;
		cursorRect.right = cursorRect.left + 4;
		cursorRect.top = mouseY - 2;
		cursorRect.bottom = cursorRect.top + 4;			
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
