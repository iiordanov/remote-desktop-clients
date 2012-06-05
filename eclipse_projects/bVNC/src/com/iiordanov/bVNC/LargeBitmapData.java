/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import java.io.IOException;

import com.iiordanov.android.drawing.OverlappingCopy;
import com.iiordanov.android.drawing.RectList;
import com.iiordanov.util.ObjectPool;

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
class LargeBitmapData extends AbstractBitmapData {
	
	/**
	 * Multiply this times total number of pixels to get estimate of process size with all buffers plus
	 * safety factor
	 */
	static final int CAPACITY_MULTIPLIER = 21;
	
	int xoffset;
	int yoffset;
	int scrolledToX;
	int scrolledToY;
	private Rect bitmapRect;
	private Paint defaultPaint;
	private RectList invalidList;
	private RectList pendingList;
	private int capacity;
	private int displayWidth;
	private int displayHeight;
	
	/**
	 * Pool of temporary rectangle objects.  Need to synchronize externally access from
	 * multiple threads.
	 */
	private static ObjectPool<Rect> rectPool = new ObjectPool<Rect>() {

		/* (non-Javadoc)
		 * @see com.antlersoft.util.ObjectPool#itemForPool()
		 */
		@Override
		protected Rect itemForPool() {
			return new Rect();
		}		
	};
	
	class LargeBitmapDrawable extends AbstractBitmapDrawable
	{
		LargeBitmapDrawable()
		{
			super(LargeBitmapData.this);
		}
		/* (non-Javadoc)
		 * @see android.graphics.drawable.DrawableContainer#draw(android.graphics.Canvas)
		 */
		@Override
		public void draw(Canvas canvas) {
			//android.util.Log.i("LBM", "Drawing "+xoffset+" "+yoffset);
			int xoff, yoff;
			synchronized ( LargeBitmapData.this )
			{
				xoff=xoffset;
				yoff=yoffset;
			}
			draw(canvas, xoff, yoff);
		}
	}
	
	/**
	 * 
	 * @param p Protocol implementation
	 * @param c View that will display screen
	 * @param displayWidth
	 * @param displayHeight
	 * @param capacity Max process heap size in bytes
	 */
	LargeBitmapData(RfbConnectable p, VncCanvas c, int displayWidth, int displayHeight, int capacity)
	{
		super(p,c);
		double scaleMultiplier = Math.sqrt((double)(capacity * 1024 * 1024) / (double)(CAPACITY_MULTIPLIER * framebufferwidth * framebufferheight));
		if (scaleMultiplier > 1)
			scaleMultiplier = 1;
		bitmapwidth=(int)((double)framebufferwidth * scaleMultiplier);
		if (bitmapwidth < displayWidth)
			bitmapwidth = displayWidth;
		bitmapheight=(int)((double)framebufferheight * scaleMultiplier);
		if (bitmapheight < displayHeight)
			bitmapheight = displayHeight;
		android.util.Log.i("LBM", "bitmapsize = ("+bitmapwidth+","+bitmapheight+")");
		mbitmap = null;
		memGraphics = null;
		bitmapPixels = null;
		invalidList = null;
		pendingList = null;
		bitmapRect = null;
		defaultPaint = null;
		System.gc();
		mbitmap = Bitmap.createBitmap(bitmapwidth, bitmapheight, Bitmap.Config.RGB_565);
		memGraphics = new Canvas(mbitmap);
		bitmapPixels = new int[bitmapwidth * bitmapheight];
		invalidList = new RectList(rectPool);
		pendingList = new RectList(rectPool);
		bitmapRect = new Rect(0,0,bitmapwidth,bitmapheight);
		defaultPaint = new Paint();
		this.capacity = capacity;
		this.displayWidth = displayWidth;
		this.displayHeight = displayHeight;
	}
	
	@Override
	AbstractBitmapDrawable createDrawable()
	{
		return new LargeBitmapDrawable();
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
			mbitmap.getPixels(bitmapPixels, srcOffset, bitmapwidth, src.left-xoffset, y-yoffset, dstW, 1);
			System.arraycopy(bitmapPixels, srcOffset, bitmapPixels, dstOffset, dstW);
			dstY += deltaY;
		}
		updateBitmap(dest.left, dest.top, dstW, dstH);
	}
	
	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#drawRect(int, int, int, int, android.graphics.Paint)
	 */
	@Override
	void drawRect(int x, int y, int w, int h, Paint paint) {
		x-=xoffset;
		y-=yoffset;
		memGraphics.drawRect(x, y, x+w, y+h, paint);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#offset(int, int)
	 */
	@Override
	public int offset(int x, int y) {
		return (y - yoffset) * bitmapwidth + x - xoffset;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#scrollChanged(int, int)
	 */
	@Override
	synchronized void scrollChanged(int newx, int newy) {
		//android.util.Log.i("LBM","scroll "+newx+" "+newy);
		int newScrolledToX = scrolledToX;
		int newScrolledToY = scrolledToY;
		int visibleWidth = vncCanvas.getVisibleWidth();
		int visibleHeight = vncCanvas.getVisibleHeight();
		if (newx - xoffset < 0)
		{
			newScrolledToX = newx + visibleWidth / 2 - bitmapwidth / 2;
			if (newScrolledToX < 0)
				newScrolledToX = 0;
		}
		else if (newx - xoffset + visibleWidth > bitmapwidth)
		{
			newScrolledToX = newx + visibleWidth / 2 - bitmapwidth / 2;
			if (newScrolledToX + bitmapwidth > framebufferwidth)
				newScrolledToX = framebufferwidth - bitmapwidth;
		}
		if (newy - yoffset < 0 )
		{
			newScrolledToY = newy + visibleHeight / 2 - bitmapheight / 2;
			if (newScrolledToY < 0)
				newScrolledToY = 0;
		}
		else if (newy - yoffset + visibleHeight > bitmapheight)
		{
			newScrolledToY = newy + visibleHeight / 2 - bitmapheight / 2;
			if (newScrolledToY + bitmapheight > framebufferheight)
				newScrolledToY = framebufferheight - bitmapheight;
		}
		if (newScrolledToX != scrolledToX || newScrolledToY != scrolledToY)
		{
			scrolledToX = newScrolledToX;
			scrolledToY = newScrolledToY;
			if ( waitingForInput)
				syncScroll();
		}
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#updateBitmap(int, int, int, int)
	 */
	@Override
	public void updateBitmap(int x, int y, int w, int h) {
		mbitmap.setPixels(bitmapPixels, offset(x,y), bitmapwidth, x-xoffset, y-yoffset, w, h);
		int right  = x+w;
		int bottom = y+h;
		dirtyRect.union(x, y, right, bottom);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#validDraw(int, int, int, int)
	 */
	@Override
	synchronized boolean validDraw(int x, int y, int w, int h) {
		//android.util.Log.i("LBM", "Validate Drawing "+x+" "+y+" "+w+" "+h+" "+xoffset+" "+yoffset+" "+(x-xoffset>=0 && x-xoffset+w<=bitmapwidth && y-yoffset>=0 && y-yoffset+h<=bitmapheight));
		boolean result = x-xoffset>=0 && x-xoffset+w<=bitmapwidth && y-yoffset>=0 && y-yoffset+h<=bitmapheight;
		ObjectPool.Entry<Rect> entry = rectPool.reserve();
		Rect r = entry.get();
		r.set(x, y, x+w, y+h);
		pendingList.subtract(r);
		if ( ! result)
		{
			invalidList.add(r);
		}
		else
			invalidList.subtract(r);
		rectPool.release(entry);
		return result;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#writeFullUpdateRequest(boolean)
	 */
	@Override
	synchronized void writeFullUpdateRequest(boolean incremental) throws IOException {
		if (! incremental) {
			ObjectPool.Entry<Rect> entry = rectPool.reserve();
			Rect r = entry.get();
			r.left=xoffset;
			r.top=yoffset;
			r.right=xoffset + bitmapwidth;
			r.bottom=yoffset + bitmapheight;
			pendingList.add(r);
			invalidList.add(r);
			rectPool.release(entry);
		}
		rfb.writeFramebufferUpdateRequest(xoffset, yoffset, bitmapwidth, bitmapheight, incremental);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#syncScroll()
	 */
	@Override
	synchronized void syncScroll() {
		
		int deltaX = xoffset - scrolledToX;
		int deltaY = yoffset - scrolledToY;
		xoffset=scrolledToX;
		yoffset=scrolledToY;
		bitmapRect.top=scrolledToY;
		bitmapRect.bottom=scrolledToY+bitmapheight;
		bitmapRect.left=scrolledToX;
		bitmapRect.right=scrolledToX+bitmapwidth;
		invalidList.intersect(bitmapRect);
		if ( deltaX != 0 || deltaY != 0)
		{
			boolean didOverlapping = false;
			if (Math.abs(deltaX) < bitmapwidth && Math.abs(deltaY) < bitmapheight) {
				ObjectPool.Entry<Rect> sourceEntry = rectPool.reserve();
				ObjectPool.Entry<Rect> addedEntry = rectPool.reserve();
				try
				{
					Rect added = addedEntry.get();
					Rect sourceRect = sourceEntry.get();
					sourceRect.set(deltaX<0 ? -deltaX : 0,
							deltaY<0 ? -deltaY : 0,
							deltaX<0 ? bitmapwidth : bitmapwidth - deltaX,
							deltaY < 0 ? bitmapheight : bitmapheight - deltaY);
					if (! invalidList.testIntersect(sourceRect)) {
						didOverlapping = true;
						OverlappingCopy.Copy(mbitmap, memGraphics, defaultPaint, sourceRect, deltaX + sourceRect.left, deltaY + sourceRect.top, rectPool);
						// Write request for side pixels
						if (deltaX != 0) {
							added.left = deltaX < 0 ? bitmapRect.right + deltaX : bitmapRect.left;
							added.right = added.left + Math.abs(deltaX);
							added.top = bitmapRect.top;
							added.bottom = bitmapRect.bottom;
							invalidList.add(added);
						}
						if (deltaY != 0) {
							added.left = deltaX < 0 ? bitmapRect.left : bitmapRect.left + deltaX;
							added.top = deltaY < 0 ? bitmapRect.bottom + deltaY : bitmapRect.top;
							added.right = added.left + bitmapwidth - Math.abs(deltaX);
							added.bottom = added.top + Math.abs(deltaY);
							invalidList.add(added);
						}
					}
				}
				finally {
					rectPool.release(addedEntry);
					rectPool.release(sourceEntry);
				}
			}
			if (! didOverlapping)
			{
				try
				{
					//android.util.Log.i("LBM","update req "+xoffset+" "+yoffset);
					mbitmap.eraseColor(Color.GREEN);
					writeFullUpdateRequest(false);
				}
				catch ( IOException ioe)
				{
					// TODO log this
				}
			}
		}
		int size = pendingList.getSize();
		for (int i=0; i<size; i++) {
			invalidList.subtract(pendingList.get(i));
		}
		size = invalidList.getSize();
		for (int i=0; i<size; i++) {
			Rect invalidRect = invalidList.get(i);
			rfb.writeFramebufferUpdateRequest(invalidRect.left, invalidRect.top, invalidRect.right-invalidRect.left, invalidRect.bottom-invalidRect.top, false);
			pendingList.add(invalidRect);
		}
		waitingForInput=true;
		//android.util.Log.i("LBM", "pending "+pendingList.toString() + "invalid "+invalidList.toString());
	}
	
	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractBitmapData#frameBufferSizeChanged(RfbProto)
	 */
	@Override
	public void frameBufferSizeChanged () {
		xoffset = 0;
		yoffset = 0;
		scrolledToX = 0;
		scrolledToY = 0;
		framebufferwidth  = rfb.framebufferWidth();
		framebufferheight = rfb.framebufferHeight();
		double scaleMultiplier = Math.sqrt((double)(capacity * 1024 * 1024) /
								(double)(CAPACITY_MULTIPLIER * framebufferwidth * framebufferheight));
		if (scaleMultiplier > 1)
			scaleMultiplier = 1;
		bitmapwidth=(int)((double)framebufferwidth * scaleMultiplier);
		if (bitmapwidth < displayWidth)
			bitmapwidth = displayWidth;
		bitmapheight=(int)((double)framebufferheight * scaleMultiplier);
		if (bitmapheight < displayHeight)
			bitmapheight = displayHeight;
		android.util.Log.i("LBM", "bitmapsize changed = ("+bitmapwidth+","+bitmapheight+")");
		mbitmap = Bitmap.createBitmap(bitmapwidth, bitmapheight, Bitmap.Config.RGB_565);
		memGraphics = new Canvas(mbitmap);
		bitmapPixels = new int[bitmapwidth * bitmapheight];
		invalidList = new RectList(rectPool);
		pendingList = new RectList(rectPool);
		bitmapRect = new Rect(0, 0, bitmapwidth, bitmapheight);
		defaultPaint = new Paint();
	}

/*
	public void fillRect(int x, int y, int w, int h, int pix) {
		try {
		if (!validDraw(x, y, w, h))
			return;
		} catch (Exception e) {
			Log.e("LBM", "There was a problem verifying whether draw is valid.");
			return;
		}

		int offset;
		for (int ry = y; ry < y + h; ry++)
			for (int rx = x; rx < x + w; rx++) {
				offset = offset(rx,ry);
				if (offset < bitmapPixels.length)
					bitmapPixels[offset] = pix;
				else
					return;
			}
		updateBitmap(x, y, w, h);
	}

	public void imageRect(int x, int y, int w, int h, int[] pix) {
		try {
		if (!validDraw(x, y, w, h))
			return;
		} catch (Exception e) {
			Log.e("LBM", "There was a problem verifying whether draw is valid.");
			return;
		}

		for (int j = 0; j < h; j++) {
			if (bitmapwidth * (y + j) + x + w < bitmapPixels.length)
				System.arraycopy(pix, (w * j), bitmapPixels, bitmapwidth * (y + j) + x, w);
			else
				return;
		}
		updateBitmap(x, y, w, h);
	}
*/
}
