/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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


package com.undatech.opaque.input;

import android.os.Vibrator;
import android.widget.RelativeLayout;
import android.widget.ImageButton;
import android.view.MotionEvent;
import android.view.View.OnClickListener;
import android.view.View;

import com.undatech.opaque.R;
import com.undatech.opaque.RemoteCanvas;
import com.undatech.opaque.RemoteCanvasActivity;
import com.undatech.opaque.input.RemotePointer;

public class InputHandlerSingleHanded extends InputHandlerDirectSwipePan {
    static final String TAG = "InputHandlerSingleHanded";
    public static final String ID = "SingleHanded";
    private RelativeLayout singleHandOpts;
    private ImageButton dragModeButton;
    private ImageButton rightDragModeButton;
    private ImageButton middleDragModeButton;
    private ImageButton scrollButton;
    private ImageButton zoomButton;
    private ImageButton cancelButton;
    int accumulatedScroll;

    private int eventStartX, eventStartY, eventAction, eventMeta;
    private boolean needInitPan;
    
    public InputHandlerSingleHanded(RemoteCanvasActivity activity, RemoteCanvas canvas, Vibrator myVibrator) {
        super(activity, canvas, myVibrator);
        initializeButtons();
    }

    /**
     * Initializes the on-screen single-handed mode-selector buttons.
     */
    private void initializeButtons() {
        singleHandOpts = (RelativeLayout) activity.findViewById(R.id.singleHandOpts);
        dragModeButton = (ImageButton) activity.findViewById(R.id.singleDrag);
        dragModeButton.setOnClickListener(new OnClickListener () {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                dragMode = true;
                RemotePointer p = canvas.getPointer();
                p.leftButtonDown(eventStartX, eventStartY, eventMeta);
                canvas.displayShortToastMessage(R.string.single_left);
            }
        });
        
        rightDragModeButton = (ImageButton) activity.findViewById(R.id.singleRight);
        rightDragModeButton.setOnClickListener(new OnClickListener () {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                rightDragMode = true;
                RemotePointer p = canvas.getPointer();
                p.rightButtonDown(eventStartX, eventStartY, eventMeta);
                canvas.displayShortToastMessage(R.string.single_right);
            }
        });
        
        middleDragModeButton = (ImageButton) activity.findViewById(R.id.singleMiddle);
        middleDragModeButton.setOnClickListener(new OnClickListener () {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                middleDragMode = true;
                RemotePointer p = canvas.getPointer();
                p.middleButtonDown(eventStartX, eventStartY, eventMeta);
                canvas.displayShortToastMessage(R.string.single_middle);
            }
        });
        
        scrollButton = (ImageButton) activity.findViewById(R.id.singleScroll);
        scrollButton.setOnClickListener(new OnClickListener () {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                canvas.cursorBeingMoved = true;
                inSwiping = true;
                RemotePointer p = canvas.getPointer();
                p.moveMouseButtonUp(eventStartX, eventStartY, eventMeta);
                canvas.displayShortToastMessage(R.string.single_scroll);
            }
        });
        
        zoomButton = (ImageButton) activity.findViewById(R.id.singleZoom);
        zoomButton.setOnClickListener(new OnClickListener () {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                inScaling = true;
                canvas.displayShortToastMessage(R.string.single_zoom);
            }
        });
        
        cancelButton = (ImageButton) activity.findViewById(R.id.singleCancel);
        cancelButton.setOnClickListener(new OnClickListener () {
            @Override
            public void onClick(View arg0) {
                singleHandOpts.setVisibility(View.GONE);
                canvas.displayShortToastMessage(R.string.single_cancel);
            }
        });
    }

    /**
     * Indicates the start of a single-handed gesture.
     */
    private void startNewSingleHandedGesture() {
        singleHandOpts.setVisibility(View.GONE);
        endDragModesAndScrolling ();
        singleHandedGesture = true;
        accumulatedScroll = 0;
    }

    /*
     * (non-Javadoc)
     * @see com.undatech.opaque.input.InputHandlerDirectSwipePan#getDescription()
     */
    @Override
    public String getDescription() {
        return canvas.getResources().getString(R.string.input_method_single_handed_description);
    }

    /*
     * (non-Javadoc)
     * @see com.undatech.opaque.input.InputHandlerDirectSwipePan#getId()
     */
    @Override
    public String getId() {
        return ID;
    }

    /*
     * (non-Javadoc)
     * @see com.undatech.opaque.input.InputHandlerGeneric#onLongPress(android.view.MotionEvent)
     */
    @Override
    public void onLongPress(MotionEvent e) {
        //android.util.Log.e(TAG, "Long press.");

        if (singleHandedGesture || singleHandedJustEnded)
            return;
    
        boolean buttonsVisible = (singleHandOpts.getVisibility() == View.VISIBLE);
        initializeSingleHandedMode(e);

        if (buttonsVisible)
            canvas.displayShortToastMessage(R.string.single_reposition);
        else
            canvas.displayShortToastMessage(R.string.single_choose);
    }

    private void initializeSingleHandedMode(MotionEvent e) {
        eventStartX   = getX(e);
        eventStartY   = getY(e);
        xInitialFocus = e.getX();
        yInitialFocus = e.getY();
        needInitPan   = true;
        eventAction   = e.getAction();
        eventMeta     = e.getMetaState();
        singleHandOpts.setVisibility(View.VISIBLE);
        
        // Move pointer to where we're performing gesture.
        RemotePointer p = canvas.getPointer();
        p.moveMouseButtonUp(eventStartX, eventStartY, eventMeta);
    }

    /*
     * (non-Javadoc)
     * @see com.undatech.opaque.input.InputHandlerGeneric#onSingleTapConfirmed(android.view.MotionEvent)
     */
    @Override
    public boolean onSingleTapConfirmed(MotionEvent e) {
        boolean buttonsVisible = (singleHandOpts.getVisibility() == View.VISIBLE);
        
        // If the single-handed gesture buttons are visible, reposition pointer.
        if (buttonsVisible) {
            initializeSingleHandedMode(e);
            canvas.displayShortToastMessage(R.string.single_reposition);
            return true;
        } else
            return super.onSingleTapConfirmed(e);
    }

    /*
     * (non-Javadoc)
     * @see com.undatech.opaque.input.InputHandlerDirectSwipePan#onScroll(android.view.MotionEvent, android.view.MotionEvent, float, float)
     */
    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {

        // If we are not in a single-handed gesture, simply pass events onto parent.
        if (!singleHandedGesture)
             return super.onScroll(e1, e2, distanceX, distanceY);
        
        // Otherwise, handle scrolling and zooming here.
        if (inSwiping) {
            scrollUp    = false;
            scrollDown  = false;
            scrollLeft  = false;
            scrollRight = false;
            // Set needed parameters for scroll event to happen in super.super.onTouchEvent.
            int absX = (int)Math.abs(distanceX);
            int absY = (int)Math.abs(distanceY);
            if (absY > absX) {
                // Scrolling up/down.
                if (distanceY > 0)
                    scrollDown  = true;
                else
                    scrollUp    = true;
                swipeSpeed = (absY + accumulatedScroll)/15;
                accumulatedScroll += absY;
            } else {
                // Scrolling side to side.
                if (distanceX > 0)
                    scrollRight = true;
                else
                    scrollLeft  = true;
                swipeSpeed = (absX + accumulatedScroll)/15;
                accumulatedScroll += absY;
            }
            if (swipeSpeed < 1) {
                swipeSpeed = 0;
            } else
                accumulatedScroll = 0;
        } else if (inScaling) {
            float scaleFactor = 1.0f + distanceY*0.01f;
            if (canvas != null && canvas.canvasZoomer != null) {
                float zoomFactor = canvas.canvasZoomer.getZoomFactor();

                if (needInitPan) {
                    needInitPan = false;
                    canvas.absolutePan((int)(canvas.getAbsX() + (xInitialFocus - canvas.getWidth()/2.f)/zoomFactor),
                                        (int)(canvas.getAbsY() + (yInitialFocus - canvas.getHeight()/2.f)/zoomFactor));
                }
                // If the scale factor actually changed, then pan to compensate for zoom.
                if (canvas.canvasZoomer.changeZoom(scaleFactor)) {
                    canvas.relativePan((int)(distanceY*zoomFactor), (int)(distanceY*zoomFactor));
                }
            }
        }
        
        return true;
    }
}

