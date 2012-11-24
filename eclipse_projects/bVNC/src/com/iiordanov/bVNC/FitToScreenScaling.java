/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.graphics.Matrix;
import android.widget.ImageView.ScaleType;

/**
 * @author Michael A. MacDonald
 */
class FitToScreenScaling extends AbstractScaling {
	
	static final String TAG = "FitToScreenScaling";
	
	private Matrix matrix;
	int canvasXOffset;
	int canvasYOffset;
	float scaling;
	float minimumScale;
	
	/**
	 * @param id
	 * @param scaleType
	 */
	public FitToScreenScaling() {
		super(R.id.itemFitToScreen, ScaleType.FIT_CENTER);
		matrix = new Matrix();
		scaling = 0;
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
		return false;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractScaling#isValidInputMode(int)
	 */
	@Override
	boolean isValidInputMode(int mode) {
		return true;
	}
	
	/**
	 * Call after scaling and matrix have been changed to resolve scrolling
	 * @param activity
	 */
	private void resolveZoom(VncCanvasActivity activity)
	{
		activity.vncCanvas.scrollToAbsolute();
		activity.vncCanvas.pan(0,0);
	}
	
	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractScaling#getScale()
	 */
	@Override
	float getScale() {
		return scaling;
	}

	private void resetMatrix()
	{
		matrix.reset();
		matrix.preTranslate(canvasXOffset, canvasYOffset);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractScaling#setScaleTypeForActivity(com.iiordanov.bVNC.VncCanvasActivity)
	 */
	@Override
	void setScaleTypeForActivity(VncCanvasActivity activity) {
		super.setScaleTypeForActivity(activity);
		activity.vncCanvas.absoluteXPosition = 0;
		activity.vncCanvas.absoluteYPosition = 0;
		canvasXOffset = -activity.vncCanvas.getCenteredXOffset();
		canvasYOffset = -activity.vncCanvas.getCenteredYOffset();
		activity.vncCanvas.computeShiftFromFullToView ();
		minimumScale = activity.vncCanvas.bitmapData.getMinimumScale();
		scaling = minimumScale;
		resetMatrix();
		matrix.postScale(scaling, scaling);
		activity.vncCanvas.setImageMatrix(matrix);
		resolveZoom(activity);
		activity.vncCanvas.pan(0, 0);
		activity.zoomer.setIsZoomOutEnabled(false);
		activity.zoomer.setIsZoomInEnabled(false);
	}
}
