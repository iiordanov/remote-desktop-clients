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
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.DrawableContainer;
import android.util.Log;
import android.widget.ImageView;

import com.undatech.opaque.AbstractDrawableData;
import com.undatech.opaque.Viewable;

/**
 * Abstract interface between the VncCanvas and the bitmap and pixel data buffers that actually contain
 * the data.
 * This allows for implementations that use smaller bitmaps or buffers to save memory.
 *
 * @author Michael A. MacDonald
 */
abstract public class AbstractBitmapData implements AbstractDrawableData {
    private final static String TAG = "AbstractBitmapData";

    public AbstractBitmapDrawable drawable;
    public Paint paint;
    int framebufferwidth;
    int framebufferheight;
    int bitmapwidth;
    int bitmapheight;
    Bitmap mbitmap;
    int[] bitmapPixels;
    Canvas memGraphics;
    boolean waitingForInput;
    Viewable vncCanvas;
    int xoffset = 0;
    int yoffset = 0;

    AbstractBitmapData(int width, int height, Viewable c) {
        vncCanvas = c;
        framebufferwidth = width;
        framebufferheight = height;
        drawable = createDrawable();
        paint = new Paint();
    }

    synchronized public void doneWaiting() {
        waitingForInput = false;
    }

    public void setCursorRect(int x, int y, int w, int h, int hX, int hY) {
        if (drawable != null)
            drawable.setCursorRect(x, y, w, h, hX, hY);
    }

    public void moveCursorRect(int x, int y) {
        if (drawable != null)
            drawable.moveCursorRect(x, y);
    }

    public void setSoftCursor(int[] newSoftCursorPixels) {
        if (drawable != null) {
            drawable.setSoftCursor(newSoftCursorPixels);
        }
    }

    public RectF getCursorRect() {
        if (drawable != null)
            return drawable.cursorRect;
        else // Return an empty new rectangle if drawable is null.
            return new RectF();
    }

    public boolean isNotInitSoftCursor() {
        if (drawable != null) {
            return (!drawable.softCursorInit);
        } else {
            return false;
        }
    }

    /**
     * @return The smallest scale supported by the implementation; the scale at which
     * the bitmap would be smaller than the screen
     */
    public float getMinimumScale() {
        return Math.min((float) vncCanvas.getWidth() / framebufferwidth, (float) vncCanvas.getHeight() / framebufferheight);
    }

    public boolean widthRatioLessThanHeightRatio() {
        return (float) vncCanvas.getWidth() / framebufferwidth < (float) vncCanvas.getHeight() / framebufferheight;
    }

    /**
     * Send a request through the protocol to get the data for the currently held bitmap
     *
     * @param incremental True if we want incremental update; false for full update
     */
    public void prepareFullUpdateRequest(boolean incremental) {
    }

    /**
     * Determine if a rectangle in full-frame coordinates can be drawn in the existing buffer
     *
     * @param x Top left x
     * @param y Top left y
     * @param w width (pixels)
     * @param h height (pixels)
     * @return True if entire rectangle fits into current screen buffer, false otherwise
     */
    public abstract boolean validDraw(int x, int y, int w, int h);

    /**
     * Return an offset in the bitmapPixels array of a point in full-frame coordinates
     *
     * @return Offset in bitmapPixels array of color data for that point
     */
    public abstract int offset(int x, int y);

    /**
     * Update pixels in the bitmap with data from the bitmapPixels array, positioned
     * in full-frame coordinates
     *
     * @param x Top left x
     * @param y Top left y
     * @param w width (pixels)
     * @param h height (pixels)
     */
    public abstract void updateBitmap(int x, int y, int w, int h);

    /**
     * Update pixels in the bitmap with data from the given bitmap, positioned
     * in full-frame coordinates
     *
     * @param b The bitmap to copy from.
     * @param x Top left x
     * @param y Top left y
     * @param w width (pixels)
     * @param h height (pixels)
     */
    public abstract void updateBitmap(Bitmap b, int x, int y, int w, int h);

    /**
     * Create drawable appropriate for this data
     *
     * @return drawable
     */
    abstract AbstractBitmapDrawable createDrawable();


    /**
     * Sets the canvas's drawable
     *
     * @param v ImageView displaying bitmap data
     */
    public void setImageDrawable(ImageView v) {
        v.setImageDrawable(drawable);
    }


    /**
     * Call in UI thread; tell ImageView we've changed
     *
     * @param v ImageView displaying bitmap data
     */
    public void updateView(ImageView v) {
        v.invalidate();
    }

    /**
     * Copy a rectangle from one part of the bitmap to another
     */
    public abstract void copyRect(int sx, int sy, int dx, int dy, int w, int h);

    public void fillRect(int x, int y, int w, int h, int pix) {
        paint.setColor(pix);
        drawRect(x, y, w, h, paint);
    }

    public void imageRect(int x, int y, int w, int h, int[] pix) {
        for (int j = 0; j < h; j++) {
            try {
                synchronized(this) {
                    System.arraycopy(pix, (w * j), bitmapPixels, offset(x, y + j), w);
                }
                //System.arraycopy(pix, (w * j), bitmapPixels, bitmapwidth * (y + j) + x, w);
            } catch (ArrayIndexOutOfBoundsException e) {
                // An index is out of bounds for some reason, but we try to continue.
                Log.e(TAG, "Caught an ArrayIndexOutOfBoundsException");
                Log.e(TAG, Log.getStackTraceString(e));
            }

        }
        updateBitmap(x, y, w, h);
    }

    /**
     * Draw a rectangle in the bitmap with coordinates given in full frame
     *
     * @param x     Top left x
     * @param y     Top left y
     * @param w     width (pixels)
     * @param h     height (pixels)
     * @param paint How to draw
     */
    public abstract void drawRect(int x, int y, int w, int h, Paint paint);

    /**
     * Scroll position has changed.
     * <p>
     * This method is called in the UI thread-- it updates internal status, but does
     * not change the bitmap data or send a network request until syncScroll is called
     *
     * @param newx Position of left edge of visible part in full-frame coordinates
     * @param newy Position of top edge of visible part in full-frame coordinates
     */
    public abstract void scrollChanged(int newx, int newy);

    /**
     * Sync scroll -- called from network thread; copies scroll changes from UI to network state
     */
    public abstract void syncScroll();

    /**
     * Release resources
     */
    public void dispose() {
        synchronized(this) {
            if (drawable != null) {
                drawable.stopDrawing();
                drawable.dispose();
            }
            if (mbitmap != null) {
                mbitmap.recycle();
                mbitmap = null;
            }
            drawable = null;
            memGraphics = null;
        }
    }

    public int fbWidth() {
        return framebufferwidth;
    }

    public int fbHeight() {
        return framebufferheight;
    }

    public int bmWidth() {
        return bitmapwidth;
    }

    public int bmHeight() {
        return bitmapheight;
    }

    public int getXoffset() {
        return xoffset;
    }

    public int getYoffset() {
        return yoffset;
    }

    public int[] getBitmapPixels() {
        return bitmapPixels;
    }

    public int getBitmapWidth() {
        return bitmapwidth;
    }

    public int getBitmapHeight() {
        return bitmapheight;
    }

    @Override
    public int getFramebufferWidth() {
        return framebufferwidth;
    }

    @Override
    public int getFramebufferHeight() {
        return framebufferheight;
    }

    public Paint getPaint() {
        return paint;
    }

    public Bitmap getMbitmap() {
        return mbitmap;
    }

    public class AbstractBitmapDrawable extends DrawableContainer {
        public Paint _defaultPaint;
        RectF cursorRect;
        int hotX, hotY;
        Bitmap softCursor;
        boolean softCursorInit;
        Rect clipRect;
        Rect toDraw;
        volatile boolean drawing = false;
        AbstractBitmapData data;
        Paint _whitePaint;
        Paint _blackPaint;

        AbstractBitmapDrawable(AbstractBitmapData data) {
            this.data = data;
            cursorRect = new RectF();
            clipRect = new Rect();
            // Try to free up some memory.
            System.gc();
            createInitialSoftCursor();

            _defaultPaint = new Paint();
            _defaultPaint.setFilterBitmap(true);
            _whitePaint = new Paint();
            _whitePaint.setColor(0xffffffff);
            _blackPaint = new Paint();
            _blackPaint.setColor(0xff000000);
        }

        private void createInitialSoftCursor() {
            synchronized (AbstractBitmapData.this) {
                softCursor = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
                softCursorInit = false;
            }
        }

        void setCursorRect(int x, int y, float w, float h, int hX, int hY) {
            hotX = hX;
            hotY = hY;
            cursorRect.left = x - hotX;
            cursorRect.right = cursorRect.left + w;
            cursorRect.top = y - hotY;
            cursorRect.bottom = cursorRect.top + h;
        }

        void moveCursorRect(int x, int y) {
            setCursorRect(x, y, cursorRect.width(), cursorRect.height(), hotX, hotY);
        }

        void setSoftCursor(int[] newSoftCursorPixels) {
            synchronized (AbstractBitmapData.this) {
                Bitmap oldSoftCursor = softCursor;
                softCursor = Bitmap.createBitmap(newSoftCursorPixels, (int) cursorRect.width(),
                        (int) cursorRect.height(), Bitmap.Config.ARGB_8888);
                softCursorInit = true;
                oldSoftCursor.recycle();
            }
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

        void dispose() {
            synchronized (AbstractBitmapData.this) {
                drawing = false;
                if (softCursor != null) {
                    softCursor.recycle();
                    softCursor = null;
                }
                cursorRect = null;
                clipRect = null;
                toDraw = null;
            }
        }

        protected void startDrawing() {
            drawing = true;
        }

        protected void stopDrawing() {
            drawing = false;
        }
    }

}
