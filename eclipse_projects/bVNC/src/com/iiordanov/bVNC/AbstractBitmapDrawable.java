/**
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
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
		drawBitmapWithinClip (canvas, xoff, yoff, null);
		setCursorRectAndDrawIfNecessary(canvas, xoff, yoff);
	}
	
	/**
	 * Draws the bitmap within the current clip.
	 * @param canvas
	 */
	public void drawBitmapWithinClip (Canvas canvas, int xoff, int yoff, Rect toDraw) {
		canvas.drawBitmap(data.mbitmap, xoff, yoff, _defaultPaint);
	}
	
	/**
	 * Sets the local cursor Rect, the clip which contains it, and draws it if necessary.
	 * @param canvas
	 */
	void setCursorRectAndDrawIfNecessary (Canvas canvas, int xoff, int yoff) {
		int l = cursorRect.left;
		int t = cursorRect.top;
		int h = cursorRect.height();
		int w = cursorRect.width();
		int r = cursorRect.left + 4*w + 1;
		int b = cursorRect.top  + 4*h + 1;
		int N = 4;

		if(data.vncCanvas.connection.getUseLocalCursor()) {
			setCursorRect(data.vncCanvas.mouseX, data.vncCanvas.mouseY);
			drawCursor(canvas);
			// Now clean up around the cursor by drawing the bitmap just outside of it.
			Rect cleantop = new Rect (l, t - N*h, r, t);
			drawBitmapWithinClip (canvas, xoff, yoff, cleantop);
			Rect cleanbottom = new Rect (l, b, r, b+N*h);
			drawBitmapWithinClip (canvas, xoff, yoff, cleanbottom);
			Rect cleanleft = new Rect (l - N*w, t - N*h, l, b + N*h);
			drawBitmapWithinClip (canvas, xoff, yoff, cleanleft);
			Rect cleanright = new Rect (r, t - N*h, r + N*w, b + N*h);
			drawBitmapWithinClip (canvas, xoff, yoff, cleanright);
		}
	}
	
	/**
	 * Draws an easily visible local pointer made of increasingly larger rectangles
	 * in the rough shape of an arrow.
	 * @param canvas
	 */
	void drawCursor(Canvas canvas) {
		int x = cursorRect.left;
		int y = cursorRect.top;
		int h = cursorRect.height();
		int w = cursorRect.width();
		Rect zero  = new Rect (cursorRect);
		Rect one   = new Rect (x, y+h, x+2*w, y+2*h);
		Rect two   = new Rect (x, y+2*h, x+3*w, y+3*h);
		Rect three = new Rect (x, y+3*h, x+4*w, y+4*h);
		
		// Draw black rectangles
		canvas.drawRect(zero,  _blackPaint);
		canvas.drawRect(one,   _blackPaint);
		canvas.drawRect(two,   _blackPaint);
		canvas.drawRect(three, _blackPaint);
		
		// Modify Rects for drawing the white rectangles
		zero.set  (zero.left + 2,  zero.top + 2,  zero.right - 1,  zero.bottom + 1);
		one.set   (one.left + 2,   one.top + 1,   one.right - 1,   one.bottom + 1);
		two.set   (two.left + 2,   two.top + 1,   two.right - 1,   two.bottom + 1);
		three.set (three.left + 2, three.top + 1, three.right - 1, three.bottom - 1);
		
		// Draw white rectangles
		canvas.drawRect(zero,  _whitePaint);
		canvas.drawRect(one,   _whitePaint);
		canvas.drawRect(two,   _whitePaint);
		canvas.drawRect(three, _whitePaint);	

		// Draw the "stem" of the "arrow"
		// TODO: Disabled for now as it makes cleaning up very complicated.
		//Rect four  = new Rect ((int)(x+1.5*w), (int)(y+4.75*h), (int)(x+3.5*w), (int)(y+6*h));
		//canvas.drawRect(four, _blackPaint);
		//four.set (four.left + 1, four.top, four.right - 1, four.bottom - 1);
		//canvas.drawRect(four, _whitePaint);
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
