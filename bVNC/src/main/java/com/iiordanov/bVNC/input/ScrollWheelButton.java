/**
 * Copyright (C) 2023 Iordan Iordanov
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

package com.iiordanov.bVNC.input;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;

import com.undatech.opaque.InputCarriable;
import com.undatech.remoteClientUi.R;

/**
 * A custom scroll wheel button that responds to vertical and horizontal swipe gestures
 * to send scroll up/down/left/right commands to the remote desktop.
 * Prioritizes the direction with the larger swipe distance.
 */
public class ScrollWheelButton extends androidx.appcompat.widget.AppCompatImageButton {
    private static final String TAG = "ScrollWheelButton";
    private static final int MIN_SCROLL_DISTANCE = 20;
    private static final float SCROLL_SENSITIVITY = 0.1f;

    private GestureDetector gestureDetector;
    private InputCarriable remoteInput;
    private TouchInputDelegate touchInputDelegate;
    private Paint wheelPaint;
    private boolean isScrolling = false;
    private boolean isVerticalScroll = true;

    public ScrollWheelButton(Context context) {
        super(context);
        init(context);
    }

    public ScrollWheelButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public ScrollWheelButton(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context);
    }

    private void init(Context context) {
        initializeGestureDetection(context);
        initializeDrawingPaints();
        configureButtonBehavior();
        Log.d(TAG, "ScrollWheelButton initialized");
    }

    private void initializeGestureDetection(Context context) {
        gestureDetector = new GestureDetector(context, new ScrollGestureListener());
    }

    private void initializeDrawingPaints() {
        wheelPaint = createSemiTransparentWhitePaint();
    }

    private Paint createSemiTransparentWhitePaint() {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(ContextCompat.getColor(getContext(), android.R.color.white));
        paint.setAlpha(180);
        paint.setStyle(Paint.Style.FILL_AND_STROKE);
        paint.setStrokeWidth(3f);
        return paint;
    }

    private void configureButtonBehavior() {
        setBackgroundResource(R.drawable.ic_scroll_wheel_48dp);
        setClickable(true);
        setFocusable(true);
    }

    /**
     * Sets the remote input carrier for sending scroll events.
     * This must be called before the button can send scroll events.
     *
     * @param remoteInput The InputCarriable instance that provides pointer access
     */
    public void setRemoteInput(InputCarriable remoteInput) {
        this.remoteInput = remoteInput;
        Log.d(TAG, "RemoteInput set: " + (remoteInput != null));
    }

    /**
     * Sets the touch input delegate for UI operations like showing the action bar.
     *
     * @param touchInputDelegate The TouchInputDelegate instance that provides UI control
     */
    public void setTouchInputDelegate(TouchInputDelegate touchInputDelegate) {
        this.touchInputDelegate = touchInputDelegate;
        Log.d(TAG, "TouchInputDelegate set: " + (touchInputDelegate != null));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (isScrolling) {
            drawScrollDirectionIndicator(canvas);
        }
    }

    private void drawScrollDirectionIndicator(Canvas canvas) {
        int width = getWidth();
        int height = getHeight();
        int centerX = width / 2;
        int centerY = height / 2;

        if (isVerticalScroll) {
            drawVerticalScrollIndicator(canvas, centerX, centerY, width, height);
        } else {
            drawHorizontalScrollIndicator(canvas, centerX, centerY, width, height);
        }
    }

    private void drawVerticalScrollIndicator(Canvas canvas, int centerX, int centerY, int width, int height) {
        drawRectangle(canvas, centerX, centerY, (float) width / 12, (float) height / 4);
    }

    private void drawRectangle(Canvas canvas, int centerX, int centerY, float rectWidth, float rectHeight) {
        canvas.drawRect(centerX - rectWidth, centerY - rectHeight, centerX + rectWidth, centerY + rectHeight, wheelPaint);
    }

    private void drawHorizontalScrollIndicator(Canvas canvas, int centerX, int centerY, int width, int height) {
        drawRectangle(canvas, centerX, centerY, (float) width / 4, (float) height / 12);
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        boolean result = gestureDetector.onTouchEvent(event);
        handleTouchEndEvents(event);
        return result || super.onTouchEvent(event);
    }

    private void handleTouchEndEvents(MotionEvent event) {
        if (isTouchEndEvent(event)) {
            stopScrollingAndRefreshDisplay();
        }
    }

    private boolean isTouchEndEvent(MotionEvent event) {
        return event.getAction() == MotionEvent.ACTION_UP || event.getAction() == MotionEvent.ACTION_CANCEL;
    }

    private void stopScrollingAndRefreshDisplay() {
        isScrolling = false;
        invalidate();
    }

    private class ScrollGestureListener extends GestureDetector.SimpleOnGestureListener {

        @Override
        public boolean onDown(@NonNull MotionEvent e) {
            keepButtonVisible();
            return true;
        }

        @Override
        public boolean onScroll(MotionEvent e1, @NonNull MotionEvent e2, float distanceX, float distanceY) {
            if (!ScrollWheelButton.this.canProcessScrollEvent()) {
                return false;
            }

            if (!ScrollWheelButton.this.isScrollDistanceSignificant(distanceX, distanceY)) {
                return false;
            }

            return ScrollWheelButton.this.processScrollGesture(distanceX, distanceY);
        }

        @Override
        public boolean onSingleTapUp(@NonNull MotionEvent e) {
            if (!ScrollWheelButton.this.canProcessTapEvent()) {
                return false;
            }

            return ScrollWheelButton.this.handleMiddleClickTap();
        }
    }

    private void keepButtonVisible() {
        if (touchInputDelegate != null) {
            touchInputDelegate.showActionBar();
        }
    }

    private boolean canProcessScrollEvent() {
        if (remoteInput == null) {
            Log.w(TAG, "RemoteInput is null, cannot send scroll events");
            return false;
        }
        return true;
    }

    private boolean canProcessTapEvent() {
        if (remoteInput == null) {
            Log.w(TAG, "RemoteInput is null, cannot send tap events");
            return false;
        }
        return true;
    }

    private boolean isScrollDistanceSignificant(float distanceX, float distanceY) {
        return Math.abs(distanceY) >= MIN_SCROLL_DISTANCE || Math.abs(distanceX) >= MIN_SCROLL_DISTANCE;
    }

    private boolean processScrollGesture(float distanceX, float distanceY) {
        keepButtonVisible();
        startScrollingAndRefreshDisplay();
        determineScrollDirection(distanceX, distanceY);

        int scrollEvents = calculateScrollEventCount(distanceX, distanceY);
        executeScrollSequence(distanceX, distanceY, scrollEvents);

        return true;
    }

    private void startScrollingAndRefreshDisplay() {
        isScrolling = true;
        invalidate();
    }

    private void determineScrollDirection(float distanceX, float distanceY) {
        this.isVerticalScroll = Math.abs(distanceY) >= Math.abs(distanceX);
    }

    private int calculateScrollEventCount(float distanceX, float distanceY) {
        float primaryDistance = this.isVerticalScroll ? Math.abs(distanceY) : Math.abs(distanceX);
        return Math.max(1, (int) (primaryDistance * SCROLL_SENSITIVITY));
    }

    private void executeScrollSequence(float distanceX, float distanceY, int scrollEvents) {
        try {
            for (int i = 0; i < scrollEvents; i++) {
                performSingleScrollEvent(distanceX, distanceY);
            }
            releaseScrollButtonAndLog();
        } catch (Exception e) {
            Log.e(TAG, "Error sending scroll events: " + e.getMessage());
        }
    }

    private void performSingleScrollEvent(float distanceX, float distanceY) {
        if (this.isVerticalScroll) {
            performVerticalScroll(distanceY);
        } else {
            performHorizontalScroll(distanceX);
        }
    }

    private void performVerticalScroll(float distanceY) {
        if (distanceY > 0) {
            scrollUpAndMaintainPointer();
        } else {
            scrollDownAndMaintainPointer();
        }
    }

    private void performHorizontalScroll(float distanceX) {
        if (distanceX > 0) {
            scrollLeftAndMaintainPointer();
        } else {
            scrollRightAndMaintainPointer();
        }
    }

    private void scrollUpAndMaintainPointer() {
        remoteInput.getPointer().scrollUp();
        remoteInput.getPointer().moveMouseButtonUp();
        Log.d(TAG, "Scroll up executed");
    }

    private void scrollDownAndMaintainPointer() {
        remoteInput.getPointer().scrollDown();
        remoteInput.getPointer().moveMouseButtonUp();
        Log.d(TAG, "Scroll down executed");
    }

    private void scrollLeftAndMaintainPointer() {
        remoteInput.getPointer().scrollLeft();
        remoteInput.getPointer().moveMouseButtonUp();
        Log.d(TAG, "Scroll left executed");
    }

    private void scrollRightAndMaintainPointer() {
        remoteInput.getPointer().scrollRight();
        remoteInput.getPointer().moveMouseButtonUp();
        Log.d(TAG, "Scroll right executed");
    }

    private void releaseScrollButtonAndLog() {
        remoteInput.getPointer().releaseButton();
        Log.d(TAG, "Scroll sequence completed, button released");
    }

    private boolean handleMiddleClickTap() {
        keepButtonVisible();
        Log.d(TAG, "Middle click tap initiated");

        try {
            remoteInput.getPointer().middleClick();
            Log.d(TAG, "Middle click executed successfully");
            return true;
        } catch (Exception ex) {
            Log.e(TAG, "Error executing middle click: " + ex.getMessage());
            return false;
        }
    }
}