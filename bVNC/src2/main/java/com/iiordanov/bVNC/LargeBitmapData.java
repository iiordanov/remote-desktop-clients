/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC;

import com.iiordanov.android.drawing.OverlappingCopy;
import com.iiordanov.android.drawing.RectList;
import com.iiordanov.util.ObjectPool;
import com.undatech.opaque.RfbConnectable;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;

class LargeBitmapData extends AbstractBitmapData {
    
    /**
     * Multiply this times total number of pixels to get estimate of process size with all buffers plus
     * safety factor
     */
    static int CAPACITY_MULTIPLIER = 18;
    double scaleMultiplier = 0;

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
    
    class LargeBitmapDrawable extends AbstractBitmapDrawable {
        LargeBitmapDrawable() {
            super(LargeBitmapData.this);
        }
        /* (non-Javadoc)
         * @see android.graphics.drawable.DrawableContainer#draw(android.graphics.Canvas)
         */
        @Override
        public void draw(Canvas canvas) {
            //android.util.Log.i("LBM", "Drawing "+xoffset+" "+yoffset);
            int xoff, yoff;
            synchronized ( LargeBitmapData.this ) {
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
    LargeBitmapData(RfbConnectable p, RemoteCanvas c, int displayWidth, int displayHeight, int capacity) {
        super(p,c);
        this.capacity = capacity;
        this.displayWidth = displayWidth;
        this.displayHeight = displayHeight;
        initializeLargeBitmapData();
    }

    @Override
    AbstractBitmapDrawable createDrawable()    {
        return new LargeBitmapDrawable();
    }

    /**
     * 
     * @return The smallest scale supported by the implementation; the scale at which
     * the bitmap would be smaller than the screen
     */
    float getMinimumScale()    {
        return Math.max((float)vncCanvas.getWidth()/bitmapwidth, (float)vncCanvas.getHeight()/bitmapheight);
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#copyRect(android.graphics.Rect, android.graphics.Rect, android.graphics.Paint)
     */
    @Override
    public void copyRect(int sx, int sy, int dx, int dy, int w, int h) {
        int srcOffset, dstOffset;
        int dstH = h;
        int dstW = w;
        int xo, yo;
        
        int startSrcY, endSrcY, dstY, deltaY;
        if (sy > dy) {
            startSrcY = sy;
            endSrcY = sy + dstH;
            dstY = dy;
            deltaY = +1;
        } else {
            startSrcY = sy + dstH - 1;
            endSrcY = sy - 1;
            dstY = dy + dstH - 1;
            deltaY = -1;
        }
        for (int y = startSrcY; y != endSrcY; y += deltaY) {
            srcOffset = offset(sx, y);
            dstOffset = offset(dx, dstY);
            xo = sx-xoffset;
            if (xo < 0) xo = 0;
            yo = y-yoffset;
            if (yo < 0) yo = 0;
            if (sx + dstW > bitmapwidth) dstW = bitmapwidth - sx;
            try {
                mbitmap.getPixels(bitmapPixels, srcOffset, bitmapwidth, xo, yo, dstW, 1);
                System.arraycopy(bitmapPixels, srcOffset, bitmapPixels, dstOffset, dstW);
            } catch (Exception e) {
                // There was an index out of bounds exception, but we continue copying what we can. 
                e.printStackTrace();
            }
            dstY += deltaY;
        }
        updateBitmap(dx, dy, dstW, dstH);
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
        if (newx - xoffset < 0) {
            newScrolledToX = newx + visibleWidth / 2 - bitmapwidth / 2;
            if (newScrolledToX < 0)
                newScrolledToX = 0;
        } else if (newx - xoffset + visibleWidth > bitmapwidth) {
            newScrolledToX = newx + visibleWidth / 2 - bitmapwidth / 2;
            if (newScrolledToX + bitmapwidth > framebufferwidth)
                newScrolledToX = framebufferwidth - bitmapwidth;
        }
        
        if (newy - yoffset < 0 ) {
            newScrolledToY = newy + visibleHeight / 2 - bitmapheight / 2;
            if (newScrolledToY < 0)
                newScrolledToY = 0;
        } else if (newy - yoffset + visibleHeight > bitmapheight) {
            newScrolledToY = newy + visibleHeight / 2 - bitmapheight / 2;
            if (newScrolledToY + bitmapheight > framebufferheight)
                newScrolledToY = framebufferheight - bitmapheight;
        }
        
        if (newScrolledToX != scrolledToX || newScrolledToY != scrolledToY) {
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
        int xo = x-xoffset;
        if (xo < 0) xo = 0;
        int yo = y-yoffset;
        if (yo < 0) yo = 0;
        if (x + w > xoffset + bitmapwidth)  w = xoffset + bitmapwidth  - x;
        if (y + h > yoffset + bitmapheight) h = yoffset + bitmapheight - y;
        
        try {
            mbitmap.setPixels(bitmapPixels, offset(x,y), bitmapwidth, xo, yo, w, h);
        } catch (IllegalArgumentException e) {
            // Do not update the bitmap if the coordinates are out of bounds.
            e.printStackTrace();
        }
    }
    
    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#updateBitmap(Bitmap, int, int, int, int)
     */
    @Override
    public void updateBitmap(Bitmap b, int x, int y, int w, int h) {
        memGraphics.drawBitmap(b, x - xoffset, y - yoffset, null);
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#validDraw(int, int, int, int)
     */
    @Override
    public synchronized boolean validDraw(int x, int y, int w, int h) {
        boolean result = x-xoffset>=0 && x-xoffset+w<=bitmapwidth && y-yoffset>=0 && y-yoffset+h<=bitmapheight;
        //android.util.Log.e("LBM", "Validate Drawing x:"+x+" y:"+y+" w:"+w+" h:"+h+" xoff:"+xoffset+" yoff:"+yoffset+" "+(x-xoffset>=0 && x-xoffset+w<=bitmapwidth && y-yoffset>=0 && y-yoffset+h<=bitmapheight));
        ObjectPool.Entry<Rect> entry = rectPool.reserve();
        Rect r = entry.get();
        r.set(x, y, x+w, y+h);
        pendingList.subtract(r);
        if (!result)
            invalidList.add(r);
        else
            invalidList.subtract(r);
        rectPool.release(entry);
        return result;
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#prepareFullUpdateRequest(boolean)
     */
    @Override
    public synchronized void prepareFullUpdateRequest(boolean incremental) {
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
                mbitmap.eraseColor(Color.GREEN);
                vncCanvas.writeFullUpdateRequest(false);
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
        initializeLargeBitmapData();
    }
    
    /**
     * This function initializes the LBM, increasing the CAPACITY_MULTIPLIER until it doesn't run out of memory while
     * initializing.
     */
    void initializeLargeBitmapData() {
        boolean tryAgain = true;
        while (CAPACITY_MULTIPLIER <= 30) {
            try {
                allocateObjects();
                // If we got to this point without throwing an out of memory exception, we break out of the loop.
                tryAgain = false;
                break;
            } catch (Throwable e) {
                // We ran out of memory, so try adjusting CAPACITY_MULTIPLIER
                CAPACITY_MULTIPLIER = CAPACITY_MULTIPLIER + 10;
                // Try to free up some memory.
                System.gc();
                // Wait a second for the system to recover.
                try { Thread.sleep(500); } catch (InterruptedException e1) { }
            }
        }
        if (tryAgain) {
            // Try forcing scaleMultiplier to be 1
            CAPACITY_MULTIPLIER = 1000;
            allocateObjects();
        }
    }
    
    void allocateObjects () {
        dispose();
        invalidList  = null;
        pendingList  = null;
        bitmapRect   = null;
        defaultPaint = null;
        // Try to free up some memory.
        System.gc();
        scaleMultiplier = Math.sqrt((double)(capacity * 1024 * 1024) /
                                    (double)(CAPACITY_MULTIPLIER * framebufferwidth * framebufferheight));
        if (scaleMultiplier > 1)
            scaleMultiplier = 1;
        bitmapwidth=(int)((double)framebufferwidth * scaleMultiplier);
        if (bitmapwidth < displayWidth*1.2)
            bitmapwidth  = (int)(displayWidth*1.2);
        bitmapheight=(int)((double)framebufferheight * scaleMultiplier);
        if (bitmapheight < displayHeight*1.2)
            bitmapheight = (int)(displayHeight*1.2);
        android.util.Log.i("LBM", "bitmapsize = ("+bitmapwidth+","+bitmapheight+")");
        mbitmap      = Bitmap.createBitmap(bitmapwidth, bitmapheight, Bitmap.Config.RGB_565);
        memGraphics  = new Canvas(mbitmap);
        bitmapPixels = new int[bitmapwidth * bitmapheight];
        invalidList  = new RectList(rectPool);
        pendingList  = new RectList(rectPool);
        bitmapRect   = new Rect(0, 0, bitmapwidth, bitmapheight);
        defaultPaint = new Paint();
        drawable     = createDrawable();
        drawable.startDrawing();
    }
}
