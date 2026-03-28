/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
 * Copyright (C) 2004 Horizon Wimba.  All Rights Reserved.
 * Copyright (C) 2001-2003 HorizonLive.com, Inc.  All Rights Reserved.
 * Copyright (C) 2001,2002 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
 * Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
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

//
// RemoteCanvas is a subclass of android.view.SurfaceView which draws a VNC
// desktop on it.
//

package com.iiordanov.bVNC;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import androidx.appcompat.widget.AppCompatImageView;

import com.google.android.material.snackbar.Snackbar;

import java.util.concurrent.CountDownLatch;
import com.iiordanov.android.bc.BCFactory;
import com.iiordanov.bVNC.input.TouchInputHandlerTouchpad;
import com.undatech.opaque.AbstractDrawableData;
import com.undatech.opaque.Connection;
import com.undatech.opaque.DrawableReallocatedListener;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.Viewable;
import com.undatech.opaque.input.RemotePointer;
import com.undatech.remoteClientUi.R;

public class RemoteCanvas extends AppCompatImageView implements Viewable {
    private final static String TAG = "RemoteCanvas";

    public AbstractScaling canvasZoomer;

    // Variable indicating that we are currently scrolling in simulated touchpad mode.
    public boolean cursorBeingMoved = false;

    // Connection parameters
    public Connection connection;
    // VNC protocol connection
    public AbstractDrawableData myDrawable;
    // Progress dialog shown at connection time.
    public Runnable setModes;

    /**
     * Handler for the dialogs that display the x509/RDP/SSH key signatures to the user.
     * Also shows the dialogs which show various connection failures.
     */
    public Handler handler;

    // Handler on main thread for layout-stability detection in waitUntilInflated().
    // We use Handler.postDelayed() to cancel/reschedule the stable-latch trigger
    // each time onSizeChanged() fires, so we only unblock after 150ms of stability.
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final CountDownLatch layoutStableLatch = new CountDownLatch(1);
    private final Runnable layoutStableRunnable = () -> layoutStableLatch.countDown();

    // The remote pointer and keyboard
    RemotePointer pointer;
    boolean useFull = false;
    boolean compact = false;

    /*
     * Position of the top left portion of the <i>visible</i> part of the screen, in
     * full-frame coordinates
     */
    int absoluteXPosition = 0, absoluteYPosition = 0;

    /*
     * How much to shift coordinates over when converting from full to view coordinates.
     */
    float shiftX = 0, shiftY = 0;

    /*
     * This variable holds the height of the visible rectangle of the screen. It is used to keep track
     * of how much of the screen is hidden by the soft keyboard if any.
     */
    int visibleHeight = -1;

    /*
     * These variables contain the width and height of the display in pixels
     */
    int displayWidth;
    int displayHeight;
    float displayDensity;

    /*
     * This flag indicates whether this is the VNC client.
     */
    boolean isVnc;

    /*
     * This flag indicates whether this is the RDP client.
     */
    boolean isRdp;

    /*
     * This flag indicates whether this is the SPICE client.
     */
    boolean isSpice;

    /*
     * This flag indicates whether this is the Opaque client.
     */
    boolean isOpaque;
    long lastDraw;

    boolean isForegrounded = false;

    /**
     * This runnable displays a message on the screen.
     */
    CharSequence screenMessage;

    DrawableReallocatedListener drawableReallocatedListener;

    /**
     * Shows a non-fatal error message.
     */
    Runnable showDialogMessage = new Runnable() {
        public void run() {
            Utils.showErrorMessage(getContext(), String.valueOf(screenMessage));
        }
    };

    //Log.d(TAG, "invalidateCanvasRunnable");
    Runnable invalidateCanvasRunnable = this::postInvalidate;

    /**
     * This runnable sets the drawable (contained in myDrawable) for the RemoteCanvas (ImageView).
     */
    private final Runnable drawableSetter = new Runnable() {
        public void run() {
            synchronized (RemoteCanvas.this) {
                Log.d(TAG, "drawableSetter.run");
                if (myDrawable != null) {
                    Log.d(TAG, "drawableSetter myDrawable not null");
                    myDrawable.setImageDrawable(RemoteCanvas.this);
                } else {
                    Log.e(TAG, "drawableSetter myDrawable is null");
                    displayOnScreenMessageLongDuration(R.string.error_out_of_memory);
                }
            }
        }
    };
    private final Runnable showMessage = new Runnable() {
        public void run() {
            Snackbar.make(RemoteCanvas.this, screenMessage, Snackbar.LENGTH_SHORT).show();
        }
    };

    private final Runnable showLongMessage = new Runnable() {
        public void run() {
            Snackbar.make(RemoteCanvas.this, screenMessage, Snackbar.LENGTH_LONG).show();
        }
    };

    /**
     * Constructor used by the inflation apparatus
     */
    public RemoteCanvas(final Context context, AttributeSet attrs) {
        super(context, attrs);

        isVnc = Utils.isVnc(getContext());
        isRdp = Utils.isRdp(getContext());
        isSpice = Utils.isSpice(getContext());
        isOpaque = Utils.isOpaque(getContext());

        final Display display = ((Activity) context).getWindow().getWindowManager().getDefaultDisplay();
        displayWidth = display.getWidth();
        displayHeight = display.getHeight();
        DisplayMetrics metrics = new DisplayMetrics();
        display.getMetrics(metrics);
        displayDensity = metrics.density;
    }

    public void setParameters(
            DrawableReallocatedListener drawableReallocatedListener,
            Connection connection,
            Handler handler,
            RemotePointer pointer,
            Runnable setModes
    ) {
        this.drawableReallocatedListener = drawableReallocatedListener;
        this.connection = connection;
        this.handler = handler;
        this.pointer = pointer;
        this.setModes = setModes;
    }

    /**
     * Retrieves the requested remote width.
     */
    @Override
    public int getDesiredWidth() {
        int w = getRemoteWidth(getWidth(), getHeight());
        if (!connection.isRequestingNewDisplayResolution() &&
                connection.getRdpResType() == RemoteClientLibConstants.RDP_GEOM_SELECT_CUSTOM) {
            w = connection.getRdpWidth();
        }
        Log.d(TAG, "Width requested: " + w);
        return w;
    }

    /**
     * Retrieves the requested remote height.
     */
    @Override
    public int getDesiredHeight() {
        int h = getRemoteHeight(getWidth(), getHeight());
        if (!connection.isRequestingNewDisplayResolution() &&
                connection.getRdpResType() == RemoteClientLibConstants.RDP_GEOM_SELECT_CUSTOM) {
            h = connection.getRdpHeight();
        }
        Log.d(TAG, "Height requested: " + h);
        return h;
    }

    /**
     * Retrieves the requested remote width.
     */
    public int getRemoteWidth(int viewWidth, int viewHeight) {
        int remoteWidth;
        int reqWidth = connection.getRdpWidth();
        int reqHeight = connection.getRdpHeight();
        if (connection.getRdpResType() == RemoteClientLibConstants.RDP_GEOM_SELECT_CUSTOM &&
                reqWidth >= 2 && reqHeight >= 2) {
            remoteWidth = reqWidth;
        } else if (connection.getRdpResType() == RemoteClientLibConstants.RDP_GEOM_SELECT_NATIVE_PORTRAIT) {
            remoteWidth = Math.min(viewWidth, viewHeight);
        } else if (connection.getRdpResType() == RemoteClientLibConstants.RDP_GEOM_SELECT_NATIVE_LANDSCAPE) {
            remoteWidth = Math.max(viewWidth, viewHeight);
        } else {
            remoteWidth = viewWidth;
        }
        // We make the resolution even if it is odd.
        if (remoteWidth % 2 == 1) remoteWidth--;
        return remoteWidth;
    }

    /**
     * Retrieves the requested remote height.
     */
    public int getRemoteHeight(int viewWidth, int viewHeight) {
        int remoteHeight;
        int reqWidth = connection.getRdpWidth();
        int reqHeight = connection.getRdpHeight();
        if (connection.getRdpResType() == RemoteClientLibConstants.RDP_GEOM_SELECT_CUSTOM &&
                reqWidth >= 2 && reqHeight >= 2) {
            remoteHeight = reqHeight;
        } else if (connection.getRdpResType() == RemoteClientLibConstants.RDP_GEOM_SELECT_NATIVE_PORTRAIT) {
            remoteHeight = Math.max(viewWidth, viewHeight);
        } else if (connection.getRdpResType() == RemoteClientLibConstants.RDP_GEOM_SELECT_NATIVE_LANDSCAPE) {
            remoteHeight = Math.min(viewWidth, viewHeight);
        } else {
            remoteHeight = viewHeight;
        }
        // We make the resolution even if it is odd.
        if (remoteHeight % 2 == 1) remoteHeight--;
        return remoteHeight;
    }

    @Override
    public void writeScreenshotToFile(String filePath, int dstWidth) {
        Utils.writeScreenshotToFile(myDrawable, filePath, dstWidth);
    }

    void showMessage(final String error) {
        Log.d(TAG, "showMessage");
        screenMessage = error;
        postMessage(showDialogMessage);
    }

    /**
     * Initializes the drawable and bitmap into which the remote desktop is drawn.
     */
    @Override
    public void reallocateDrawable(int dx, int dy) {
        Log.i(TAG, "Desktop size is " + dx + " x " + dy);

        int fbSize = dx * dy;

        // Internal bitmap data
        int capacity = BCFactory.getInstance().getBCActivityManager().getMemoryClass(Utils.getActivityManager(getContext()));

        if (connection.getForceFull() == BitmapImplHint.AUTO) {
            if (fbSize * CompactBitmapData.CAPACITY_MULTIPLIER <= capacity * 1024 * 1024) {
                useFull = true;
                compact = true;
            } else {
                useFull = true;
            }
        } else {
            useFull = (connection.getForceFull() == BitmapImplHint.FULL);
        }

        reallocateMyDrawable(dx, dy);

        try {
            if (needsLocalCursor()) {
                initializeSoftCursor();
            }
            postDrawableSetter();
            handler.post(setModes);
            syncScroll();
            drawableReallocatedListener.setBitmapData(myDrawable);
        } catch (NullPointerException e) {
            Log.e(TAG, "Caught a NullPointerException");
            Log.e(TAG, Log.getStackTraceString(e));
        }
    }

    private void reallocateMyDrawable(int dx, int dy) {
        synchronized(this) {
            if (!isVnc) {
                Log.i(TAG, "Using UltraCompactBufferBitmapData.");
                myDrawable = new UltraCompactBitmapData(dx, dy, this, isSpice | isOpaque);
            } else {
                try {
                    if (!compact) {
                        Log.i(TAG, "Using FullBufferBitmapData.");
                        myDrawable = new FullBufferBitmapData(dx, dy, this);
                    } else {
                        Log.i(TAG, "Using CompactBufferBitmapData.");
                        myDrawable = new CompactBitmapData(dx, dy, this, isSpice | isOpaque);
                    }
                } catch (Throwable e) { // If despite our efforts we fail to allocate memory, use CompactBitmapData.
                    Log.e(TAG, "Could not allocate drawable, attempting to use CompactBitmapData.");
                    if (myDrawable != null) {
                        myDrawable.dispose();
                    }
                    myDrawable = new CompactBitmapData(dx, dy, this, isSpice | isOpaque);
                }
            }
        }
    }

    /**
     * Determines if the app should show a local cursor or not
     */
    private boolean needsLocalCursor() {
        boolean isRdpSpiceOrOpaque = isRdp || isSpice || isOpaque;
        boolean localCursorNotForceDisabled =
                connection.getUseLocalCursor() != Constants.CURSOR_FORCE_DISABLE;
        boolean localCursorForceEnabled =
                connection.getUseLocalCursor() == Constants.CURSOR_FORCE_LOCAL;
        return (isRdpSpiceOrOpaque && localCursorNotForceDisabled) || localCursorForceEnabled;
    }

    @Override
    public void postDrawableSetter() {
        handler.post(drawableSetter);
    }


    @Override
    public void prepareFullUpdateRequest(boolean incremental) {
        synchronized (this) {
            if (myDrawable != null) {
                myDrawable.prepareFullUpdateRequest(incremental);
            }
        }
    }

    @Override
    public int getXoffset() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.getXoffset();
            }
            return 0;
        }
    }

    @Override
    public int getYoffset() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.getYoffset();
            }
            return 0;
        }
    }

    @Override
    public int bmWidth() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.bmWidth();
            }
            return 0;
         }
    }

    @Override
    public int bmHeight() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.bmHeight();
            }
            return 0;
        }
    }

    /**
     * Displays a message on the screen for a short duration.
     */
    public void displayOnScreenMessageShortDuration(final CharSequence message) {
        screenMessage = message;
        postMessage(showMessage);
    }

    @Override
    public void displayOnScreenMessageLongDuration(final CharSequence message) {
        screenMessage = message;
        postMessage(showLongMessage);
    }

    /**
     * Displays a message on the screen for a short duration
     */
    @Override
    public void displayOnScreenMessageShortDuration(final int messageID) {
        screenMessage = getResources().getText(messageID);
        postMessage(showMessage);
    }

    @Override
    public void displayOnScreenMessageLongDuration(final int messageID) {
        screenMessage = getResources().getText(messageID);
        postMessage(showLongMessage);
    }

    private void postMessage(Runnable showMessage) {
        handler.removeCallbacks(showMessage);
        handler.post(showMessage);
    }

    /**
     * Lets the drawable know that an update from the remote server has arrived.
     */
    public void doneWaiting() {
        synchronized (this) {
            if (myDrawable != null) {
                myDrawable.doneWaiting();
            }
        }
    }

    /**
     * Indicates that RemoteCanvas's scroll position should be synchronized with the
     * drawable's scroll position (used only in LargeBitmapData)
     */
    public void syncScroll() {
        synchronized (this) {
            if (myDrawable != null) {
                myDrawable.syncScroll();
            }
        }
    }

    @Override
    public boolean isUseFull() {
        return useFull;
    }

    /**
     * Computes the X and Y offset for converting coordinates from full-frame coordinates to view coordinates.
     * Delegates to the scale-aware overload with scale=1.0f (used by OneToOneScaling).
     */
    public void computeShiftFromFullToView() {
        computeShiftFromFullToView(1.0f);
    }

    /**
     * Computes the X and Y shift for the given scale factor so that the scaled image is centered in the view.
     * The formula ensures the image center maps to the view center at any scale:
     *   shiftX = fbWidth/2 - viewWidth/(2*scale)
     * At scale=1 this reduces to (fbWidth - viewWidth)/2, identical to the original formula.
     * @param scale the current zoom/scale factor
     */
    public void computeShiftFromFullToView(float scale) {
        synchronized (this) {
            if (myDrawable != null) {
                shiftX = myDrawable.fbWidth() / 2.0f - getWidth() / (2.0f * scale);
                shiftY = myDrawable.fbHeight() / 2.0f - getHeight() / (2.0f * scale);
            }
        }
    }

    /**
     * Change to Canvas's scroll position to match the absoluteXPosition
     */
    void resetScroll() {
        float scale = getZoomFactor();
        int scrollX = (int) ((absoluteXPosition - shiftX) * scale);
        int scrollY = (int) ((absoluteYPosition - shiftY) * scale);

        // When the viewport is larger than the framebuffer on an axis, zero the scroll
        // and reset the absolute position on that axis so the matrix translation
        // (which centers the image) is not overridden by a residual scroll value.
        synchronized (this) {
            if (myDrawable != null) {
                if (myDrawable.fbWidth() <= getVisibleDesktopWidth()) {
                    scrollX = 0;
                    absoluteXPosition = 0;
                }
                if (myDrawable.fbHeight() <= getVisibleDesktopHeight()) {
                    // Image fits in visible area. But keyboard/extra keys may have
                    // reduced the visible area below the full view height. The matrix
                    // centers the image in the full view, which can push the bottom
                    // below the visible boundary. Compute a scroll to shift up.
                    if (visibleHeight > 0 && visibleHeight < getHeight()) {
                        int imageBottomPx = (int) (scale * (myDrawable.fbHeight() - shiftY));
                        scrollY = Math.max(0, imageBottomPx - visibleHeight);
                        // Don't scroll past the image top (top = -shiftY * scale)
                        int maxScroll = Math.max(0, (int) (-shiftY * scale));
                        scrollY = Math.min(scrollY, maxScroll);
                    } else {
                        scrollY = 0;
                    }
                    absoluteYPosition = 0;
                }
            }
        }

        Log.d(TAG, "resetScroll: " + scrollX + ", " + scrollY);
        scrollTo(scrollX, scrollY);
    }

    private boolean contentExceedsVisibleArea() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.fbWidth() > getVisibleDesktopWidth()
                        || myDrawable.fbHeight() > getVisibleDesktopHeight();
            }
        }
        return false;
    }

    /**
     * Make sure mouse is visible on displayable part of screen
     */
    public void movePanToMakePointerVisible() {
        //Log.d(TAG, "movePanToMakePointerVisible");
        boolean panX = true;
        boolean panY = true;

        // Don't pan in a certain direction if dimension scaled is already less
        // than the dimension of the visible part of the screen.
        synchronized (this) {
            if (myDrawable != null) {
                if (myDrawable.fbWidth() < getVisibleDesktopWidth()) {
                    panX = false;
                }
                if (myDrawable.fbHeight() < getVisibleDesktopHeight()) {
                    panY = false;
                }
            }
        }

        // We only pan if the current scaling is able to pan, or content exceeds
        // visible area (e.g., IME or extra keys reduce available space).
        if (canvasZoomer != null && !canvasZoomer.isAbleToPan() && !contentExceedsVisibleArea())
            return;

        int x = pointer.getX();
        int y = pointer.getY();
        boolean panned = false;
        int w = getVisibleDesktopWidth();
        int h = getVisibleDesktopHeight();
        int iw = getImageWidth();
        int ih = getImageHeight();
        int wThresh = Math.min(Constants.H_THRESH, w / 4);
        int hThresh = Math.min(Constants.W_THRESH, h / 4);

        int newX = absoluteXPosition;
        int newY = absoluteYPosition;

        if (x - absoluteXPosition >= w - wThresh) {
            newX = x - (w - wThresh);
            if (newX + w > iw)
                newX = iw - w;
        } else if (x < absoluteXPosition + wThresh) {
            newX = x - wThresh;
            if (newX < 0)
                newX = 0;
        }
        if (panX && newX != absoluteXPosition) {
            absoluteXPosition = newX;
            panned = true;
        }

        if (y - absoluteYPosition >= h - hThresh) {
            newY = y - (h - hThresh);
            if (newY + h > ih)
                newY = ih - h;
        } else if (y < absoluteYPosition + hThresh) {
            newY = y - hThresh;
            if (newY < 0)
                newY = 0;
        }
        if (panY && newY != absoluteYPosition) {
            absoluteYPosition = newY;
            panned = true;
        }

        if (panned) {
            //scrollBy(newX - absoluteXPosition, newY - absoluteYPosition);
            resetScroll();
        }
    }

    /**
     * Pan by a number of pixels (relative pan)
     * @return True if the pan changed the view (did not move view out of bounds); false otherwise
     */
    public boolean relativePan(float dX, float dY) {
        Log.d(TAG, "relativePan: " + dX + ", " + dY);

        // We only pan if the current scaling is able to pan, or content exceeds
        // visible area (e.g., IME or extra keys reduce available space).
        if (canvasZoomer != null && !canvasZoomer.isAbleToPan() && !contentExceedsVisibleArea())
            return false;

        double scale = getZoomFactor();

        double sX = (double) dX / scale;
        double sY = (double) dY / scale;

        // Prevent panning beyond the desktop image edges.
        if (absoluteXPosition + sX < 0)
            sX = -absoluteXPosition;
        if (absoluteYPosition + sY < 0)
            sY = -absoluteYPosition;

        if (absoluteXPosition + getVisibleDesktopWidth() + sX > getImageWidth())
            sX = getImageWidth() - getVisibleDesktopWidth() - absoluteXPosition;
        if (absoluteYPosition + getVisibleDesktopHeight() + sY > getImageHeight())
            sY = getImageHeight() - getVisibleDesktopHeight() - absoluteYPosition;

        absoluteXPosition += (int) sX;
        absoluteYPosition += (int) sY;
        resetScroll();
        return true;
    }

    /**
     * Absolute pan.
     */
    public void absolutePan(int x, int y) {
        //Log.d(TAG, "absolutePan: " + x + ", " + y);

        if (canvasZoomer != null) {
            int vW = getVisibleDesktopWidth();
            int vH = getVisibleDesktopHeight();
            int w = getImageWidth();
            int h = getImageHeight();
            if (x + vW > w) x = w - vW;
            if (y + vH > h) y = h - vH;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            absoluteXPosition = x;
            absoluteYPosition = y;
            resetScroll();
        }
    }

    /* (non-Javadoc)
     * @see android.view.View#onScrollChanged(int, int, int, int)
     */
    @Override
    protected void onScrollChanged(int l, int t, int oldL, int oldT) {
        super.onScrollChanged(l, t, oldL, oldT);
        synchronized (this) {
            if (myDrawable != null) {
                myDrawable.scrollChanged(absoluteXPosition, absoluteYPosition);
            }
        }
    }

    @Override
    public Bitmap getBitmap() {
        synchronized (this) {
            Bitmap bitmap = null;
            if (myDrawable != null) {
                bitmap = myDrawable.getMbitmap();
            }
            return bitmap;
        }
    }

    /**
     * Causes a redraw of the myDrawable to happen at the indicated coordinates.
     */
    public void reDraw(int x, int y, int w, int h) {
        //Log.i(TAG, "reDraw called: " + x + ", " + y + " + " + w + "x" + h);
        long timeNow = System.currentTimeMillis();
        if (timeNow - lastDraw > 16.6666) {
            float scale = getZoomFactor();
            float shiftedX = x - shiftX;
            float shiftedY = y - shiftY;
            // Make the box slightly larger to avoid artifacts due to truncation errors.
            postInvalidate((int) ((shiftedX - 1) * scale), (int) ((shiftedY - 1) * scale),
                    (int) ((shiftedX + w + 1) * scale), (int) ((shiftedY + h + 1) * scale));
            lastDraw = timeNow;
        } else {
            handler.removeCallbacks(invalidateCanvasRunnable);
            handler.postDelayed(invalidateCanvasRunnable, 100);
        }
    }

    /**
     * This is a float-accepting version of reDraw().
     * Causes a redraw of the myDrawable to happen at the indicated coordinates.
     */
    public void reDraw(float x, float y, float w, float h) {
        //Log.i(TAG, "reDraw float called: " + x + ", " + y + " + " + w + "x" + h);
        long timeNow = System.currentTimeMillis();
        if (timeNow - lastDraw > 16.6666) {
            float scale = getZoomFactor();
            float shiftedX = x - shiftX;
            float shiftedY = y - shiftY;
            // Make the box slightly larger to avoid artifacts due to truncation errors.
            postInvalidate((int) ((shiftedX - 1.f) * scale), (int) ((shiftedY - 1.f) * scale),
                    (int) ((shiftedX + w + 1.f) * scale), (int) ((shiftedY + h + 1.f) * scale));
            lastDraw = timeNow;
        } else {
            handler.removeCallbacks(invalidateCanvasRunnable);
            handler.postDelayed(invalidateCanvasRunnable, 100);
        }
    }

    /**
     * Invalidates (to redraw) the location of the remote pointer.
     */
    public void invalidateMousePosition() {
        synchronized (this) {
            if (myDrawable != null) {
                moveSoftCursor(pointer.getX(), pointer.getY());
                RectF r = getCursorRect();
                reDraw(r.left, r.top, r.width(), r.height());
            }
        }
    }

    @Override
    public void setMousePointerPosition(int x, int y) {
        softCursorMove(x, y);
    }

    @Override
    public void mouseMode(boolean relative) {
        if (relative && !connection.getInputMode().equals(TouchInputHandlerTouchpad.ID)) {
            showMessage(getContext().getString(R.string.info_set_touchpad_input_mode));
        } else {
            this.pointer.setRelativeEvents(relative);
        }
    }

    @Override
    public boolean isAbleToPan() {
        return canvasZoomer.isAbleToPan();
    }

    /**
     * Moves soft cursor into a particular location.
     */
    synchronized public void softCursorMove(int x, int y) {
        if (isNotInitSoftCursor() && connection.getUseLocalCursor() != Constants.CURSOR_FORCE_DISABLE) {
            initializeSoftCursor();
        }

        if (!isCursorBeingMoved() || pointer.isRelativeEvents()) {
            pointer.setX(x);
            pointer.setY(y);
            RectF prevR = new RectF(getCursorRect());
            // Move the cursor.
            moveSoftCursor(x, y);
            // Show the cursor.
            RectF r = getCursorRect();
            reDraw(r.left, r.top, r.width(), r.height());
            reDraw(prevR.left, prevR.top, prevR.width(), prevR.height());
        }
    }

    private RectF getCursorRect() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.getCursorRect();
            }
            return new RectF();
        }
    }

    private void moveSoftCursor(int x, int y) {
        synchronized (this) {
            if (myDrawable != null) {
                myDrawable.moveCursorRect(x, y);
            }
        }
    }

    private boolean isNotInitSoftCursor() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.isNotInitSoftCursor();
            }
            return false;
        }
    }

    /**
     * Initializes the data structure which holds the remote pointer data.
     */
    void initializeSoftCursor() {
        Bitmap bm = BitmapFactory.decodeResource(getResources(), R.drawable.cursor);
        int w = bm.getWidth();
        int h = bm.getHeight();
        int[] tempPixels = new int[w * h];
        bm.getPixels(tempPixels, 0, w, 0, 0, w, h);
        // Set cursor rectangle as well.
        setSoftCursorRectAndPixels(w, h, tempPixels);
        bm.recycle();
    }

    private void setSoftCursorRectAndPixels(int w, int h, int[] tempPixels) {
        synchronized (this) {
            if (myDrawable != null) {
                myDrawable.setCursorRect(pointer.getX(), pointer.getY(), w, h, 0, 0);
                // Set softCursor to whatever the resource is.
                myDrawable.setSoftCursor(tempPixels);
            }
        }
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        Log.d(TAG, "onCreateInputConnection called");
        BaseInputConnection bic = new BaseInputConnection(this, false);
        outAttrs.actionLabel = null;
        outAttrs.inputType = getKeyboardVariation();
        String currentIme = Settings.Secure.getString(getContext().getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
        Log.d(TAG, "currentIme: " + currentIme);
        outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return bic;
    }

    private int getKeyboardVariation() {
        String keyboardVariationStr = Utils.querySharedPreferenceString(
                getContext(),
                Constants.softwareKeyboardType,
                getContext().getString(R.string.pref_keyboard_type_TYPE_NULL_value)
        );
        int keyboardVariation = 0;
        try {
            keyboardVariation = Integer.parseInt(keyboardVariationStr);
        } catch (NumberFormatException e) {
            Log.e(TAG, e.toString());
        }
        return keyboardVariation;
    }

    public float getZoomFactor() {
        if (canvasZoomer == null)
            return 1;
        return canvasZoomer.getZoomFactor();
    }

    public int getVisibleDesktopWidth() {
        return (int) ((double) getWidth() / getZoomFactor() + 0.5);
    }

    public int getVisibleDesktopHeight() {
        if (visibleHeight > 0)
            return (int) ((double) visibleHeight / getZoomFactor() + 0.5);
        else
            return (int) ((double) getHeight() / getZoomFactor() + 0.5);
    }

    public void setVisibleDesktopHeight(int newHeight) {
        visibleHeight = newHeight;
    }

    public int getImageWidth() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.fbWidth();
            }
            return 0;
        }
    }

    public int getImageHeight() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.fbHeight();
            }
            return 0;
        }
    }

    public int getCenteredXOffset() {
        synchronized (this) {
            if (myDrawable != null) {
                return (myDrawable.fbWidth() - getWidth()) / 2;
            }
            return 0;
        }
    }

    public int getCenteredYOffset() {
        synchronized (this) {
            if (myDrawable != null) {
                return (myDrawable.fbHeight() - getHeight()) / 2;
            }
            return 0;
        }
    }

    public float getMinimumScale() {
        synchronized (this) {
            if (myDrawable != null) {
                return myDrawable.getMinimumScale();
            }
            return 1.f;
        }
    }

    public float getDisplayDensity() {
        return displayDensity;
    }

    public boolean getMouseFollowPan() {
        return connection.getFollowPan();
    }

    public int getAbsX() {
        return absoluteXPosition;
    }

    public int getAbsY() {
        return absoluteYPosition;
    }

    @Override
    public float getShiftX() {
        return shiftX;
    }

    @Override
    public float getShiftY() {
        return shiftY;
    }

    @Override
    public boolean relativePan(int deltaX, int deltaY) {
        return relativePan((float) deltaX, (float) deltaY);
    }

    @Override
    public boolean isCursorBeingMoved() {
        return cursorBeingMoved;
    }

    @Override
    public void setCursorBeingMoved(boolean cursorBeingMoved) {
        this.cursorBeingMoved = cursorBeingMoved;
    }

    @Override
    public void changeZoom(float scaleFactor, float fx, float fy) {
        if (canvasZoomer != null && getContext() instanceof RemoteCanvasActivity) {
            canvasZoomer.changeZoom((RemoteCanvasActivity) getContext(), scaleFactor, fx, fy);
        }
    }
    @Override
    public boolean isZoomerAbleToPan() {
        return canvasZoomer != null && canvasZoomer.isAbleToPan();
    }

    @Override
    public Handler getHandler() {
        return handler;
    }

    /**
     * Waits until the view has stable, non-zero dimensions.
     *
     * In simple cases (phone full-screen) the view is measured once and
     * stabilises immediately. In DeX/freeform mode the window can go through
     * several layout passes (initial size → adjusted for taskbar/decorations).
     * We wait until no further onSizeChanged() events arrive for 150ms, which
     * guarantees the connection thread captures the final settled dimensions.
     */
    public void waitUntilInflated() {
        try {
            layoutStableLatch.await();
        } catch (InterruptedException e) {
            Log.e(TAG, Log.getStackTraceString(e));
        }
    }

    /**
     * Used to detect when the view is inflated to a sane size other than 0x0.
     */
    @Override
    protected void onSizeChanged(int w, int h, int oldW, int oldH) {
        if (w > 0 && h > 0) {
            // Reschedule the layout-stable trigger on every dimension change.
            // In DeX/freeform mode the window can go through multiple layout passes
            // (e.g., full height first, then reduced by the taskbar). By cancelling
            // and reposting, waitUntilInflated() only unblocks after 150ms of no
            // further changes — so the connection thread always sees final dimensions.
            mainHandler.removeCallbacks(layoutStableRunnable);
            mainHandler.postDelayed(layoutStableRunnable, 150);

            // Recalculate scaling parameters when view size changes
            // (DeX, freeform window, split-screen, rotation, etc.)
            // This fixes coordinate mapping so clicks land in the right place.
            if (canvasZoomer != null && myDrawable != null) {
                Log.d(TAG, "onSizeChanged: recalculating for " + w + "x" + h
                        + " (was " + oldW + "x" + oldH + ")");
                canvasZoomer.handleViewSizeChange(this);
            }
        }
    }

    @Override
    public boolean isForegrounded() {
        return isForegrounded;
    }

    public void setForegrounded(boolean foregrounded) {
        isForegrounded = foregrounded;
    }
}
