/**
 * Copyright (C) 2013- Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
import com.undatech.opaque.Viewable;

import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import com.undatech.opaque.InputCarriable;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.remoteClientUi.R;

public class TouchInputHandlerSingleHanded extends TouchInputHandlerDirectSwipePan {
    public static final String ID = "SINGLE_HANDED_MODE";
    static final String TAG = "InputHandlerSingleHand";
    int accumulatedScroll;
    private RelativeLayout singleHandOpts;
    private ImageButton dragModeButton;
    private ImageButton rightDragModeButton;
    private ImageButton middleDragModeButton;
    private ImageButton scrollButton;
    private ImageButton zoomButton;
    private ImageButton cancelButton;
    private int eventStartX, eventStartY, eventAction, eventMeta;
    private boolean needInitPan;

    public TouchInputHandlerSingleHanded(TouchInputDelegate touchInputDelegate, Viewable viewable,
                                         InputCarriable remoteInput, boolean debugLogging,
                                         int swipeSpeed) {
        super(touchInputDelegate, viewable, remoteInput, debugLogging, swipeSpeed);
        initializeButtons();
    }

    /**
     * Initializes the on-screen single-handed mode-selector buttons.
     */
    private void initializeButtons() {
        singleHandOpts = touchInputDelegate.findViewById(R.id.singleHandOpts);
        dragModeButton = touchInputDelegate.findViewById(R.id.singleDrag);
        dragModeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                dragMode = true;
                remoteInput.getPointer().leftButtonDown(eventStartX, eventStartY, eventMeta);
                viewable.displayShortToastMessage(R.string.single_left);
            }
        });

        rightDragModeButton = touchInputDelegate.findViewById(R.id.singleRight);
        rightDragModeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                rightDragMode = true;
                remoteInput.getPointer().rightButtonDown(eventStartX, eventStartY, eventMeta);
                viewable.displayShortToastMessage(R.string.single_right);
            }
        });

        middleDragModeButton = touchInputDelegate.findViewById(R.id.singleMiddle);
        middleDragModeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                middleDragMode = true;
                remoteInput.getPointer().middleButtonDown(eventStartX, eventStartY, eventMeta);
                viewable.displayShortToastMessage(R.string.single_middle);
            }
        });

        scrollButton = touchInputDelegate.findViewById(R.id.singleScroll);
        scrollButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View arg0) {
                GeneralUtils.debugLog(debugLogging, TAG, "scrollButton clicked. Setting inSwiping to true.");
                startNewSingleHandedGesture();
                viewable.setCursorBeingMoved(true);
                inSwiping = true;
                remoteInput.getPointer().moveMouseButtonUp(eventStartX, eventStartY, eventMeta);
                viewable.displayShortToastMessage(R.string.single_scroll);
            }
        });

        zoomButton = touchInputDelegate.findViewById(R.id.singleZoom);
        zoomButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                inScaling = true;
                viewable.displayShortToastMessage(R.string.single_zoom);
            }
        });

        cancelButton = touchInputDelegate.findViewById(R.id.singleCancel);
        cancelButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View arg0) {
                singleHandOpts.setVisibility(View.GONE);
                viewable.displayShortToastMessage(R.string.single_cancel);
            }
        });
    }

    /**
     * Indicates the start of a single-handed gesture.
     */
    private void startNewSingleHandedGesture() {
        GeneralUtils.debugLog(debugLogging, TAG, "startNewSingleHandedGesture");
        singleHandOpts.setVisibility(View.GONE);
        endDragModesAndScrolling();
        singleHandedGesture = true;
        accumulatedScroll = 0;
    }

    /*
     * (non-Javadoc)
     * @see com.iiordanov.bVNC.input.InputHandlerDirectSwipePan#getDescription()
     */
    @Override
    public String getDescription() {
        return viewable.getResources().getString(R.string.input_method_single_handed_description);
    }

    /*
     * (non-Javadoc)
     * @see com.iiordanov.bVNC.input.InputHandlerDirectSwipePan#getId()
     */
    @Override
    public String getId() {
        return ID;
    }

    /*
     * (non-Javadoc)
     * @see com.iiordanov.bVNC.input.InputHandlerGeneric#onLongPress(android.view.MotionEvent)
     */
    @Override
    public void onLongPress(MotionEvent e) {
        GeneralUtils.debugLog(debugLogging, TAG, "onLongPress, e: " + e);

        if (singleHandedGesture || singleHandedJustEnded)
            return;

        boolean buttonsVisible = (singleHandOpts.getVisibility() == View.VISIBLE);
        initializeSingleHandedMode(e);

        if (buttonsVisible)
            viewable.displayShortToastMessage(R.string.single_reposition);
        else
            viewable.displayShortToastMessage(R.string.single_choose);
    }

    private void initializeSingleHandedMode(MotionEvent e) {
        eventStartX = getX(e);
        eventStartY = getY(e);
        xInitialFocus = e.getX();
        yInitialFocus = e.getY();
        needInitPan = true;
        eventAction = e.getAction();
        eventMeta = e.getMetaState();
        singleHandOpts.setVisibility(View.VISIBLE);

        // Move pointer to where we're performing gesture.
        remoteInput.getPointer().moveMouseButtonUp(eventStartX, eventStartY, eventMeta);
    }

    /*
     * (non-Javadoc)
     * @see com.iiordanov.bVNC.input.InputHandlerGeneric#onSingleTapConfirmed(android.view.MotionEvent)
     */
    @Override
    public boolean onSingleTapConfirmed(MotionEvent e) {
        GeneralUtils.debugLog(debugLogging, TAG, "onSingleTapConfirmed");

        boolean buttonsVisible = (singleHandOpts.getVisibility() == View.VISIBLE);

        // If the single-handed gesture buttons are visible, reposition pointer.
        if (buttonsVisible) {
            initializeSingleHandedMode(e);
            viewable.displayShortToastMessage(R.string.single_reposition);
            return true;
        } else
            return super.onSingleTapConfirmed(e);
    }

    /*
     * (non-Javadoc)
     * @see com.iiordanov.bVNC.input.InputHandlerDirectSwipePan#onScroll(android.view.MotionEvent, android.view.MotionEvent, float, float)
     */
    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        GeneralUtils.debugLog(debugLogging, TAG, "onScroll, e1: " + e1 + ", e2:" + e2);

        if (consumeAsMouseWheel(e1, e2)) {
            return true;
        }

        // If we are not in a single-handed gesture, simply pass events onto parent.
        if (!singleHandedGesture)
            return super.onScroll(e1, e2, distanceX, distanceY);

        // Otherwise, handle scrolling and zooming here.
        if (inSwiping) {
            GeneralUtils.debugLog(debugLogging, TAG, "inSwiping");
            scrollUp = false;
            scrollDown = false;
            scrollLeft = false;
            scrollRight = false;
            // Set needed parameters for scroll event to happen in super.super.onTouchEvent.
            int absX = (int) Math.abs(distanceX);
            int absY = (int) Math.abs(distanceY);
            if (absY > absX) {
                // Scrolling up/down.
                if (distanceY > 0)
                    scrollDown = true;
                else
                    scrollUp = true;
                swipeSpeed = (absY + accumulatedScroll) / 15;
                accumulatedScroll += absY;
            } else {
                // Scrolling side to side.
                if (distanceX > 0)
                    scrollRight = true;
                else
                    scrollLeft = true;
                swipeSpeed = (absX + accumulatedScroll) / 15;
                accumulatedScroll += absY;
            }
            if (swipeSpeed < 1) {
                swipeSpeed = 0;
            } else
                accumulatedScroll = 0;
        } else if (inScaling) {
            GeneralUtils.debugLog(debugLogging, TAG, "inScaling");
            float scaleFactor = 1.0f + distanceY * 0.01f;
            if (viewable != null) {
                float zoomFactor = viewable.getZoomFactor();

                if (needInitPan) {
                    needInitPan = false;
                    viewable.absolutePan((int) (viewable.getAbsX() + (xInitialFocus - viewable.getWidth() / 2.f) / zoomFactor),
                            (int) (viewable.getAbsY() + (yInitialFocus - viewable.getHeight() / 2.f) / zoomFactor));
                }
                viewable.changeZoom(scaleFactor, xInitialFocus, yInitialFocus);
            }
        }

        return true;
    }
}

