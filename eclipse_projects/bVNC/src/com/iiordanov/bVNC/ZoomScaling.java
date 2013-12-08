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

import android.graphics.Matrix;
import android.util.Log;
import android.widget.Toast;
import android.widget.ImageView.ScaleType;

/**
 * @author Michael A. MacDonald
 */
class ZoomScaling extends AbstractScaling {
    
    static final String TAG = "ZoomScaling";

    private Matrix matrix;
    int canvasXOffset;
    int canvasYOffset;
    float scaling;
    float minimumScale;
    
    /**
     * @param id
     * @param scaleType
     */
    public ZoomScaling() {
        super(R.id.itemZoomable, ScaleType.MATRIX);
        matrix = new Matrix();
        scaling = 1;
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractScaling#getDefaultHandlerId()
     */
    @Override
    int getDefaultHandlerId() {
        return R.id.itemInputTouchPanZoomMouse;
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractScaling#isAbleToPan()
     */
    @Override
    boolean isAbleToPan() {
        return true;
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractScaling#isValidInputMode(int)
     */
    @Override
    boolean isValidInputMode(int mode) {
//        return mode == R.id.itemInputTouchPanZoomMouse;
        return true;
    }
    
    /**
     * Call after scaling and matrix have been changed to resolve scrolling
     * @param activity
     */
    private void resolveZoom(RemoteCanvas canvas)
    {
        canvas.scrollToAbsolute();
        //activity.vncCanvas.pan(0,0);
    }
    
    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractScaling#zoomIn(com.iiordanov.bVNC.VncCanvasActivity)
     */
    @Override
    void zoomIn(RemoteCanvasActivity activity) {
        resetMatrix();
        standardizeScaling();
        scaling += 0.25;
        if (scaling > 4.0)
        {
            scaling = (float)4.0;
            activity.zoomer.setIsZoomInEnabled(false);
        }
        activity.zoomer.setIsZoomOutEnabled(true);
        matrix.postScale(scaling, scaling);
        //Log.v(TAG,String.format("before set matrix scrollx = %d scrolly = %d", activity.vncCanvas.getScrollX(), activity.vncCanvas.getScrollY()));
        activity.getCanvas().setImageMatrix(matrix);
        resolveZoom(activity.getCanvas());
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractScaling#getScale()
     */
    @Override
    float getScale() {
        return scaling;
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractScaling#zoomOut(com.iiordanov.bVNC.VncCanvasActivity)
     */
    @Override
    void zoomOut(RemoteCanvasActivity activity) {
        resetMatrix();
        standardizeScaling();
        scaling -= 0.25;
        if (scaling < minimumScale)
        {
            scaling = minimumScale;
            activity.zoomer.setIsZoomOutEnabled(false);
        }
        activity.zoomer.setIsZoomInEnabled(true);
        matrix.postScale(scaling, scaling);
        //Log.v(TAG,String.format("before set matrix scrollx = %d scrolly = %d", activity.vncCanvas.getScrollX(), activity.vncCanvas.getScrollY()));
        activity.getCanvas().setImageMatrix(matrix);
        //Log.v(TAG,String.format("after set matrix scrollx = %d scrolly = %d", activity.vncCanvas.getScrollX(), activity.vncCanvas.getScrollY()));
        resolveZoom(activity.getCanvas());
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractScaling#adjust(com.iiordanov.bVNC.VncCanvasActivity, float, float, float)
     */
    @Override
    public void adjust(RemoteCanvasActivity activity, float scaleFactor, float fx, float fy) {
        
        float oldScale;
        float newScale = scaleFactor * scaling;
        if (scaleFactor < 1)
        {
            if (newScale < minimumScale)
            {
                newScale = minimumScale;
                activity.zoomer.setIsZoomOutEnabled(false);
            }
            activity.zoomer.setIsZoomInEnabled(true);
        }
        else
        {
            if (newScale > 4)
            {
                newScale = 4;
                activity.zoomer.setIsZoomInEnabled(false);
            }
            activity.zoomer.setIsZoomOutEnabled(true);
        }
        
        RemoteCanvas canvas = activity.getCanvas();
        // ax is the absolute x of the focus
        int xPan = canvas.absoluteXPosition;
        float ax = (fx / scaling) + xPan;
        float newXPan = (scaling * xPan - scaling * ax + newScale * ax)/newScale;
        int yPan = canvas.absoluteYPosition;
        float ay = (fy / scaling) + yPan;
        float newYPan = (scaling * yPan - scaling * ay + newScale * ay)/newScale;
        
        // Here we do snapping to 1:1. If we are approaching scale = 1, we snap to it.
        oldScale = scaling;
        if ( (newScale > 0.90f && newScale < 1.00f) ||
             (newScale > 1.00f && newScale < 1.10f) ) {
            newScale = 1.f;
            // Only if oldScale is outside the snap region, do we inform the user.
            if (oldScale < 0.90f || oldScale > 1.10f)
                canvas.displayShortToastMessage(R.string.snap_one_to_one);
        }
        
        resetMatrix();
        scaling = newScale;
        matrix.postScale(scaling, scaling);
        canvas.setImageMatrix(matrix);
        resolveZoom(canvas);
        
        // Only if we have actually scaled do we pan and potentially set mouse position.
        if (oldScale != newScale) {
            canvas.pan((int)(newXPan - xPan), (int)(newYPan - yPan));
            canvas.getPointer().mouseFollowPan();
        }
    }    
    
    private void resetMatrix()
    {
        matrix.reset();
        matrix.preTranslate(canvasXOffset, canvasYOffset);
    }
    
    /**
     *  Set scaling to one of the clicks on the zoom scale
     */
    private void standardizeScaling()
    {
        scaling = ((float)((int)(scaling * 4))) / 4;
    }

    /* (non-Javadoc)
     * @see com.iiordanov.bVNC.AbstractScaling#setScaleTypeForActivity(com.iiordanov.bVNC.VncCanvasActivity)
     */
    @Override
    void setScaleTypeForActivity(RemoteCanvasActivity activity) {
        super.setScaleTypeForActivity(activity);
        RemoteCanvas canvas = activity.getCanvas();
        canvasXOffset = -canvas.getCenteredXOffset();
        canvasYOffset = -canvas.getCenteredYOffset();
        canvas.computeShiftFromFullToView ();
        minimumScale = canvas.getMinimumScale();
        scaling = 1.f;
        resetMatrix();
        canvas.setImageMatrix(matrix);
        resolveZoom(canvas);
    }

}
