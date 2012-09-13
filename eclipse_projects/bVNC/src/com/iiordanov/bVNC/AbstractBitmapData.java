/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import java.io.IOException;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.widget.ImageView;
import android.util.Log;


/**
 * Abstract interface between the VncCanvas and the bitmap and pixel data buffers that actually contain
 * the data.
 * This allows for implementations that use smaller bitmaps or buffers to save memory. 
 * @author Michael A. MacDonald
 *
 */
abstract public class AbstractBitmapData {
	int framebufferwidth;
	int framebufferheight;
	int bitmapwidth;
	int bitmapheight;
	RfbConnectable rfb;
	Bitmap mbitmap;
	int bitmapPixels[];
	Canvas memGraphics;
	boolean waitingForInput;
	VncCanvas vncCanvas;
	private AbstractBitmapDrawable drawable;
	protected Paint paint;

	AbstractBitmapData(RfbConnectable p, VncCanvas c)
	{
		rfb=p;
		vncCanvas = c;
		framebufferwidth=rfb.framebufferWidth();
		framebufferheight=rfb.framebufferHeight();
		drawable = createDrawable();
		paint = new Paint();
	}
	
	synchronized void doneWaiting()
	{
		waitingForInput=false;
	}
	
	final void invalidateMousePosition()
	{
		if (vncCanvas.connection.getUseLocalCursor())
		{
			drawable.setCursorRect(vncCanvas.mouseX,vncCanvas.mouseY);
			vncCanvas.invalidate(drawable.cursorRect);
		}
	}
	
	/**
	 * 
	 * @return The smallest scale supported by the implementation; the scale at which
	 * the bitmap would be smaller than the screen
	 */
	float getMinimumScale()
	{
		return Math.min((float)vncCanvas.getWidth()/bitmapwidth, (float)vncCanvas.getHeight()/bitmapheight);
	}
	
	/**
	 * Send a request through the protocol to get the data for the currently held bitmap
	 * @param incremental True if we want incremental update; false for full update
	 */
	public abstract void writeFullUpdateRequest( boolean incremental) throws IOException;
	
	/**
	 * Determine if a rectangle in full-frame coordinates can be drawn in the existing buffer
	 * @param x Top left x
	 * @param y Top left y
	 * @param w width (pixels)
	 * @param h height (pixels)
	 * @return True if entire rectangle fits into current screen buffer, false otherwise
	 */
	public abstract boolean validDraw( int x, int y, int w, int h);
	
	/**
	 * Return an offset in the bitmapPixels array of a point in full-frame coordinates
	 * @param x
	 * @param y
	 * @return Offset in bitmapPixels array of color data for that point
	 */
	public abstract int offset( int x, int y);
	
	/**
	 * Update pixels in the bitmap with data from the bitmapPixels array, positioned
	 * in full-frame coordinates
	 * @param x Top left x
	 * @param y Top left y
	 * @param w width (pixels)
	 * @param h height (pixels)
	 */
	public abstract void updateBitmap( int x, int y, int w, int h);


	/**
	 * Create drawable appropriate for this data
	 * @return drawable
	 */
	abstract AbstractBitmapDrawable createDrawable();
	
	
	/**
	 * Sets the canvas's drawable
	 * @param v ImageView displaying bitmap data
	 */
	void setImageDrawable(ImageView v)
	{
		v.setImageDrawable(drawable);
	}
	
	
	/**
	 * Call in UI thread; tell ImageView we've changed
	 * @param v ImageView displaying bitmap data
	 */
	void updateView(ImageView v)
	{
		v.invalidate();
	}
	
	/**
	 * Copy a rectangle from one part of the bitmap to another
	 * @param src Rectangle in full-frame coordinates to be copied
	 * @param dest Destination rectangle in full-frame coordinates
	 * @param paint Paint specifier
	 */
	public abstract void copyRect( Rect src, Rect dest );

	public void fillRect(int x, int y, int w, int h, int pix) {
		paint.setColor(pix);
		drawRect(x, y, w, h, paint);
	}

	public void imageRect(int x, int y, int w, int h, int[] pix) {
		for (int j = 0; j < h; j++) {
			try {
				System.arraycopy(pix, (w * j), bitmapPixels, offset(x, y+j), w);
				//System.arraycopy(pix, (w * j), bitmapPixels, bitmapwidth * (y + j) + x, w);
			} catch (ArrayIndexOutOfBoundsException e) {
				// An index is out of bounds for some reason, but we try to continue.
			}

		}
		updateBitmap(x, y, w, h);
	}

	/**
	 * Draw a rectangle in the bitmap with coordinates given in full frame
	 * @param x Top left x
	 * @param y Top left y
	 * @param w width (pixels)
	 * @param h height (pixels)
	 * @param paint How to draw
	 */
	abstract void drawRect( int x, int y, int w, int h, Paint paint);
	
	/**
	 * Scroll position has changed.
	 * <p>
	 * This method is called in the UI thread-- it updates internal status, but does
	 * not change the bitmap data or send a network request until syncScroll is called
	 * @param newx Position of left edge of visible part in full-frame coordinates
	 * @param newy Position of top edge of visible part in full-frame coordinates
	 */
	abstract void scrollChanged( int newx, int newy);

	/**
	 * Remote framebuffer size has changed.
	 * <p>
	 * This method is called when the framebuffer has changed size and reinitializes the
	 * necessary data structures to support that change.
	 */
	public abstract void frameBufferSizeChanged ();
	
	/**
	 * Sync scroll -- called from network thread; copies scroll changes from UI to network state
	 */
	abstract void syncScroll();

	/**
	 * Release resources
	 */
	void dispose()
	{
		if ( mbitmap!=null )
			mbitmap.recycle();
		memGraphics = null;
		bitmapPixels = null;
	}
	
	public int width () {
		return framebufferwidth;
	}

	public int height () {
		return framebufferheight;
	}
}
