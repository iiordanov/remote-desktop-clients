/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * <p>
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <p>
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.Log;

class CompactBitmapData extends AbstractBitmapData {
    /**
     * Multiply this times total number of pixels to get estimate of process size with all buffers plus
     * safety factor
     */
    static final int CAPACITY_MULTIPLIER = 7;
    private final static String TAG = "CompactBitmapData";
    Bitmap.Config cfg = Bitmap.Config.RGB_565;

    CompactBitmapData(int width, int height, RemoteCanvas c, boolean trueColor) {
        super(width, height, c);
        bitmapwidth = framebufferwidth;
        bitmapheight = framebufferheight;
        // To please createBitmap, we ensure the size it at least 1x1.
        if (bitmapwidth == 0) bitmapwidth = 1;
        if (bitmapheight == 0) bitmapheight = 1;

        if (trueColor)
            cfg = Bitmap.Config.ARGB_8888;

        allocateBuffer();
        Log.i(TAG, "bitmapsize = (" + bitmapwidth + "," + bitmapheight + ")");

        if (Constants.SDK_INT >= android.os.Build.VERSION_CODES.HONEYCOMB_MR1) {
            mbitmap.setHasAlpha(false);
        }

        memGraphics = new Canvas(mbitmap);
        bitmapPixels = new int[bitmapwidth * bitmapheight];
        drawable.startDrawing();
    }

    public void allocateBuffer() {
        synchronized(this) {
            mbitmap = Bitmap.createBitmap(bitmapwidth, bitmapheight, cfg);
        }
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
        synchronized(this) {
            mbitmap.setPixels(bitmapPixels, offset(x, y), bitmapwidth, x, y, w, h);
        }
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#updateBitmap(Bitmap, int, int, int, int)
     */
    @Override
    public void updateBitmap(Bitmap b, int x, int y, int w, int h) {
        synchronized(this) {
            memGraphics.drawBitmap(b, x, y, null);
        }
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#copyRect(android.graphics.Rect, android.graphics.Rect, android.graphics.Paint)
     */
    @Override
    public void copyRect(int sx, int sy, int dx, int dy, int w, int h) {
        int srcOffset, dstOffset;
        int dstH = h;
        int dstW = w;

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
            try {
                synchronized(this) {
                    mbitmap.getPixels(bitmapPixels, srcOffset, bitmapwidth, sx - xoffset, y - yoffset, dstW, 1);
                    System.arraycopy(bitmapPixels, srcOffset, bitmapPixels, dstOffset, dstW);
                }
            } catch (Exception e) {
                // There was an index out of bounds exception, but we continue copying what we can.
                Log.e(TAG, "Caught an exception");
                Log.e(TAG, Log.getStackTraceString(e));
            }
            dstY += deltaY;
        }
        updateBitmap(dx, dy, dstW, dstH);
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#drawRect(int, int, int, int, android.graphics.Paint)
     */
    @Override
    public void drawRect(int x, int y, int w, int h, Paint paint) {
        synchronized(this) {
            memGraphics.drawRect(x, y, x + w, y + h, paint);
        }
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#scrollChanged(int, int)
     */
    @Override
    public void scrollChanged(int newx, int newy) {
        // Don't need to do anything here
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractBitmapData#syncScroll()
     */
    @Override
    public void syncScroll() {
        // Don't need anything here either
    }

    class CompactBitmapDrawable extends AbstractBitmapDrawable {

        CompactBitmapDrawable() {
            super(CompactBitmapData.this);
        }

        /* (non-Javadoc)
         * @see android.graphics.drawable.DrawableContainer#draw(android.graphics.Canvas)
         */
        @Override
        public synchronized void draw(Canvas canvas) {
            //Log.i(TAG, "draw");
            try {
                synchronized(CompactBitmapData.this) {
                    if (drawing) {
                        if (mbitmap != null && !mbitmap.isRecycled()) {
                            canvas.drawBitmap(mbitmap, 0.0f, 0.0f, _defaultPaint);
                        }
                        if (softCursor != null && !softCursor.isRecycled()) {
                            canvas.drawBitmap(softCursor, cursorRect.left, cursorRect.top, _defaultPaint);
                        }
                    }
                }
            } catch (Throwable e) {
                Log.e(TAG, "Failed to draw");
                Log.e(TAG, Log.getStackTraceString(e));
            }
        }
    }
}
