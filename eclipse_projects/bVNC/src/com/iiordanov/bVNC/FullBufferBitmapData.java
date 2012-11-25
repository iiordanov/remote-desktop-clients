/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (c) 2010 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import java.io.IOException;
import java.util.Arrays;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.Log;

/**
 * @author Michael A. MacDonald
 *
 */
class FullBufferBitmapData extends AbstractBitmapData {
	int xoffset;
	int yoffset;
	int dataWidth;
	int dataHeight;

	
	/**
	 * @author Michael A. MacDonald
	 *
	 */
	class Drawable extends AbstractBitmapDrawable {
		private final static String TAG = "Drawable";
		int drawWidth;
		int drawHeight; 
		int xo, yo;
		Paint paint;
		Rect toDraw;
		
		/**
		 * @param data
		 */
		public Drawable(AbstractBitmapData data) {
			super(data);
			paint = new Paint ();
		}

		/* (non-Javadoc)
		 * @see android.graphics.drawable.DrawableContainer#draw(android.graphics.Canvas)
		 */
		@Override
		public void draw(Canvas canvas) {
			// If the redrawn area encompasses the rectangle where the cursor was previously,
			// we consider the region cleaned and zero out the rectangle.
			if (canvas.getClipBounds().contains(preCursorRect))
				preCursorRect.setEmpty();

			toDraw = canvas.getClipBounds();
			
			// To avoid artifacts, we need to enlarge the box by one pixel in all directions.
			toDraw.set(toDraw.left-1, toDraw.top-1, toDraw.right+1, toDraw.bottom+1);
			drawWidth  = toDraw.width();
			drawHeight = toDraw.height();

			if (toDraw.left < 0)
				xo = 0;
			else if (toDraw.left >= data.framebufferwidth)
				return;
			else
				xo = toDraw.left;

			if (toDraw.top < 0)
				yo = 0;
			else if (toDraw.top >= data.framebufferheight)
				return;
			else
				yo = toDraw.top;

			if (xo + drawWidth  >= data.framebufferwidth)
				drawWidth  = data.framebufferwidth  - xo;
			if (yo + drawHeight >= data.framebufferheight)
				drawHeight = data.framebufferheight - yo;

			try {
				canvas.drawBitmap(data.bitmapPixels, offset(xo, yo), data.framebufferwidth, 
									xo, yo, drawWidth, drawHeight, false, null);

			} catch (Exception e) {
				Log.e (TAG, "Failed to draw bitmap: xo, yo/drawW, drawH: " + xo + ", " + yo + "/"
						+ drawWidth + ", " + drawHeight);
				// In case we couldn't draw for some reason, try putting up text.
				paint.setColor(Color.WHITE);
				canvas.drawText("There was a problem drawing the remote desktop on the screen. " +
						"Please disconnect and reconnect to the VNC server.", xo+50, yo+50, paint);
			}

			if (softCursor != null) {
				canvas.drawBitmap(softCursor, cursorRect.left, cursorRect.top, null);
			}
		}
	}

	/**
	 * Multiply this times total number of pixels to get estimate of process size with all buffers plus
	 * safety factor
	 */
	static final int CAPACITY_MULTIPLIER = 6;
	
	/**
	 * @param p
	 * @param c
	 */
	public FullBufferBitmapData(RfbConnectable p, VncCanvas c, int capacity) {
		super(p, c);
		framebufferwidth=rfb.framebufferWidth();
		framebufferheight=rfb.framebufferHeight();
		bitmapwidth=framebufferwidth;
		bitmapheight=framebufferheight;
		dataWidth=framebufferwidth;
		dataHeight=framebufferheight;
		android.util.Log.i("FBBM", "bitmapsize = ("+bitmapwidth+","+bitmapheight+")");
		bitmapPixels = new int[framebufferwidth * framebufferheight];
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#copyRect(android.graphics.Rect, android.graphics.Rect, android.graphics.Paint)
	 */
	@Override
	public void copyRect(Rect src, Rect dest) {
		int srcOffset, dstOffset;
		int dstH = dest.height();
		int dstW = dest.width();
		
		int startSrcY, endSrcY, dstY, deltaY;
		if (src.top > dest.top) {
			startSrcY = src.top;
			endSrcY = src.top + dstH;
			dstY = dest.top;
			deltaY = +1;
		} else {
			startSrcY = src.top + dstH - 1;
			endSrcY = src.top - 1;
			dstY = dest.top + dstH - 1;
			deltaY = -1;
		}
		for (int y = startSrcY; y != endSrcY; y += deltaY) {
			srcOffset = offset(src.left, y);
			dstOffset = offset(dest.left, dstY);
			try {
				System.arraycopy(bitmapPixels, srcOffset, bitmapPixels, dstOffset, dstW);
			} catch (Exception e) {
				// There was an index out of bounds exception, but we continue copying what we can. 
				e.printStackTrace();
			}
			dstY += deltaY;
		}
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#createDrawable()
	 */
	@Override
	AbstractBitmapDrawable createDrawable() {
		return new Drawable(this);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#drawRect(int, int, int, int, android.graphics.Paint)
	 */
	@Override
	void drawRect(int x, int y, int w, int h, Paint paint) {
		int color = paint.getColor();
		int offset = offset(x,y);
		if (w > 10)
		{
			for (int j = 0; j < h; j++, offset += framebufferwidth)
			{
				Arrays.fill(bitmapPixels, offset, offset + w, color);
			}
		}
		else
		{
			for (int j = 0; j < h; j++, offset += framebufferwidth - w)
			{
				for (int k = 0; k < w; k++, offset++)
				{
					bitmapPixels[offset] = color;
				}
			}
		}
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#offset(int, int)
	 */
	@Override
	public int offset(int x, int y) {
		return x + y * framebufferwidth;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#scrollChanged(int, int)
	 */
	@Override
	void scrollChanged(int newx, int newy) {
		xoffset = newx;
		yoffset = newy;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#frameBufferSizeChanged(RfbProto)
	 */
	@Override
	public void frameBufferSizeChanged () {
		framebufferwidth=rfb.framebufferWidth();
		framebufferheight=rfb.framebufferHeight();
		bitmapwidth=framebufferwidth;
		bitmapheight=framebufferheight;
		android.util.Log.i("FBBM", "bitmapsize changed = ("+bitmapwidth+","+bitmapheight+")");
		if ( dataWidth < framebufferwidth || dataHeight < framebufferheight ) {
			bitmapPixels = null;
			System.gc();
			dataWidth  = framebufferwidth;
			dataHeight = framebufferheight;
			bitmapPixels = new int[framebufferwidth * framebufferheight];
		}
	}
	
	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#syncScroll()
	 */
	@Override
	void syncScroll() {
		// Don't need to do anything here
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#updateBitmap(int, int, int, int)
	 */
	@Override
	public void updateBitmap(int x, int y, int w, int h) {
		// Don't need to do anything here
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#validDraw(int, int, int, int)
	 */
	@Override
	public boolean validDraw(int x, int y, int w, int h) {
	    if (x + w > bitmapwidth || y + h > bitmapheight)
	    	return false;
	    return true;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#writeFullUpdateRequest(boolean)
	 */
	@Override
	public void writeFullUpdateRequest(boolean incremental) throws IOException {
		rfb.writeFramebufferUpdateRequest(0, 0, bitmapwidth, bitmapheight, incremental);
	}

}
