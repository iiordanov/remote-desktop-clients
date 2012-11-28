/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;

/**
 * @author Michael A. MacDonald
 *
 */
class CompactBitmapData extends AbstractBitmapData {

	class CompactBitmapDrawable extends AbstractBitmapDrawable
	{
		CompactBitmapDrawable()
		{
			super(CompactBitmapData.this);
		}

		/* (non-Javadoc)
		 * @see android.graphics.drawable.DrawableContainer#draw(android.graphics.Canvas)
		 */
		@Override
		public void draw(Canvas canvas) {
			canvas.drawBitmap(data.mbitmap, 0, 0, _defaultPaint);
			canvas.drawBitmap(softCursor, cursorRect.left, cursorRect.top, null);
		}
	}
	
	CompactBitmapData(RfbConnectable rfb, VncCanvas c)
	{
		super(rfb,c);
		bitmapwidth=framebufferwidth;
		bitmapheight=framebufferheight;
		mbitmap = Bitmap.createBitmap(bitmapwidth, bitmapheight, Bitmap.Config.RGB_565);
		memGraphics = new Canvas(mbitmap);
		bitmapPixels = new int[bitmapwidth * bitmapheight];
	}

	@Override
	public boolean validDraw(int x, int y, int w, int h) {
		return true;
	}

	@Override
	public int offset(int x, int y) {
		return y * bitmapwidth + x;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#createDrawable()
	 */
	@Override
	AbstractBitmapDrawable createDrawable() {
		return new CompactBitmapDrawable();
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#updateBitmap(int, int, int, int)
	 */
	@Override
	public void updateBitmap(int x, int y, int w, int h) {
		mbitmap.setPixels(bitmapPixels, offset(x,y), bitmapwidth, x, y, w, h);
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
				mbitmap.getPixels(bitmapPixels, srcOffset, bitmapwidth, src.left-xoffset, y-yoffset, dstW, 1);
				System.arraycopy(bitmapPixels, srcOffset, bitmapPixels, dstOffset, dstW);
			} catch (Exception e) {
				// There was an index out of bounds exception, but we continue copying what we can. 
				e.printStackTrace();
			}
			dstY += deltaY;
		}
		updateBitmap(dest.left, dest.top, dstW, dstH);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#drawRect(int, int, int, int, android.graphics.Paint)
	 */
	@Override
	void drawRect(int x, int y, int w, int h, Paint paint) {
		memGraphics.drawRect(x, y, x + w, y + h, paint);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#scrollChanged(int, int)
	 */
	@Override
	void scrollChanged(int newx, int newy) {
		// Don't need to do anything here
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#frameBufferSizeChanged(RfbProto)
	 */
	@Override
	public void frameBufferSizeChanged () {
		framebufferwidth=rfb.framebufferWidth();
		framebufferheight=rfb.framebufferHeight();
		android.util.Log.i("CBM", "bitmapsize changed = ("+bitmapwidth+","+bitmapheight+")");
		if ( bitmapwidth < framebufferwidth || bitmapheight < framebufferheight ) {
			bitmapPixels = null;
			System.gc();
			bitmapwidth  = framebufferwidth;
			bitmapheight = framebufferheight;
			bitmapPixels = new int[bitmapwidth * bitmapheight];
			mbitmap = Bitmap.createBitmap(bitmapwidth, bitmapheight, Bitmap.Config.RGB_565);
			memGraphics = new Canvas(mbitmap);
		}
	}
	
	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#syncScroll()
	 */
	@Override
	void syncScroll() {
		// Don't need anything here either
		
	}
}
