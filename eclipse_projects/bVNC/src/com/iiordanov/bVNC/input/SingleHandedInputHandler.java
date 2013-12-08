package com.iiordanov.bVNC.input;

import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import com.iiordanov.bVNC.R;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.iiordanov.bVNC.R.id;
import com.iiordanov.bVNC.R.string;

public class SingleHandedInputHandler extends TouchMouseSwipePanInputHandler {
    static final String TAG = "SingleHandedInputHandler";
    static final String SINGLE_HANDED_MODE = "SINGLE_HANDED_MODE";
    private RelativeLayout singleHandOpts;
    private ImageButton dragModeButton;
    private ImageButton rightDragModeButton;
    private ImageButton middleDragModeButton;
    private ImageButton scrollButton;
    private ImageButton zoomButton;
    private ImageButton cancelButton;
    int accumulatedScroll;

    private int eventStartX, eventStartY, eventAction, eventMeta;

    /**
     * Divide stated fling velocity by this amount to get initial velocity
     * per pan interval
     */
    static final float FLING_FACTOR = 8;
    
    /**
     * @param c
     */
    public SingleHandedInputHandler(RemoteCanvasActivity va, RemoteCanvas v) {
        super(va, v);
        initializeButtons();
    }

    private void initializeButtons() {
        singleHandOpts = (RelativeLayout) activity.findViewById(R.id.singleHandOpts);
        dragModeButton = (ImageButton) activity.findViewById(R.id.singleDrag);
        dragModeButton.setOnClickListener(new OnClickListener () {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                dragMode = true;
                RemotePointer p = canvas.getPointer();
                p.processPointerEvent(eventStartX, eventStartY, eventAction, eventMeta, true, false, false, false, 0);
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
                p.processPointerEvent(eventStartX, eventStartY, eventAction, eventMeta, true, true, false, false, 0);
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
                p.processPointerEvent(eventStartX, eventStartY, eventAction, eventMeta, true, false, true, false, 0);
                canvas.displayShortToastMessage(R.string.single_middle);
            }
        });
        
        scrollButton = (ImageButton) activity.findViewById(R.id.singleScroll);
        scrollButton.setOnClickListener(new OnClickListener () {
            @Override
            public void onClick(View arg0) {
                startNewSingleHandedGesture();
                canvas.inScrolling = true;
                inSwiping = true;
                RemotePointer p = canvas.getPointer();
                p.processPointerEvent(eventStartX, eventStartY, eventAction, eventMeta, false, false, false, false, 0);
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

    private void startNewSingleHandedGesture() {
        singleHandOpts.setVisibility(View.GONE);
        endDragModesAndScrolling ();
        singleHandedGesture = true;
        accumulatedScroll = 0;
    }

    /*
     * (non-Javadoc)
     * 
     * @see com.iiordanov.bVNC.AbstractInputHandler#getHandlerDescription()
     */
    @Override
    public CharSequence getHandlerDescription() {
        return canvas.getResources().getString(R.string.input_mode_single_handed_description);
    }

    /*
     * (non-Javadoc)
     * 
     * @see com.iiordanov.bVNC.AbstractInputHandler#getName()
     */
    @Override
    public String getName() {
        return SINGLE_HANDED_MODE;
    }

    /*
     * (non-Javadoc)
     * 
     * @see android.view.GestureDetector.SimpleOnGestureListener#onLongPress(android.view.MotionEvent)
     */
    @Override
    public void onLongPress(MotionEvent e) {
        android.util.Log.e(TAG, "Long press.");

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
        eventAction   = e.getAction();
        eventMeta     = e.getMetaState();
        singleHandOpts.setVisibility(View.VISIBLE);
        
        // Move pointer to where we're performing gesture.
        RemotePointer p = canvas.getPointer();
        p.processPointerEvent(eventStartX, eventStartY, eventAction, eventMeta, false, false, false, false, 0);
    }

    /*
     * (non-Javadoc)
     * 
     * @see android.view.GestureDetector.SimpleOnGestureListener#onSingleTapConfirmed(android.view.MotionEvent)
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
     * 
     * @see android.view.GestureDetector.SimpleOnGestureListener#onScroll(android.view.MotionEvent,
     *      android.view.MotionEvent, float, float)
     */
    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {

        // If we are not in a single-handed gesture, simply pass events onto parent.
        if (!singleHandedGesture)
             return super.onScroll(e1, e2, distanceX, distanceY);
        
        // Otherwise, handle scrolling and zooming here.
        if (inSwiping) {
            twoFingerSwipeUp    = false;
            twoFingerSwipeDown  = false;
            twoFingerSwipeRight = false;
            twoFingerSwipeLeft  = false;
            // Set needed parameters for scroll event to happen in super.super.onTouchEvent.
            int absX = (int)Math.abs(distanceX);
            int absY = (int)Math.abs(distanceY);
            if (absY > absX) {
                // Scrolling up/down.
                if (distanceY > 0)
                    twoFingerSwipeDown  = true;
                else
                    twoFingerSwipeUp    = true;
                swipeSpeed = (absY+accumulatedScroll)/15;
                accumulatedScroll += absY;
            } else {
                // Scrolling side to side.
                if (distanceX > 0)
                    twoFingerSwipeRight = true;
                else
                    twoFingerSwipeLeft  = true;
                swipeSpeed = (absX+accumulatedScroll)/15;
                accumulatedScroll += absY;
            }
            if (swipeSpeed < 1) {
                swipeSpeed = 0;
            } else
                accumulatedScroll = 0;
        } else if (inScaling) {
            float scaleFactor = 1.0f + distanceY*0.01f;                
            if (canvas != null && canvas.scaling != null)
                canvas.scaling.adjust(activity, scaleFactor, xInitialFocus, yInitialFocus);
        }
        
        return true;
    }
}

