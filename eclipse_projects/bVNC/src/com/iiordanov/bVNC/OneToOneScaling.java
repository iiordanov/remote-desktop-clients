/**
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.graphics.Matrix;
import android.widget.ImageView.ScaleType;

/**
 * @author Michael A. MacDonald
 */
class OneToOneScaling extends AbstractScaling {

	static final String TAG = "OneToOneScaling";

	private Matrix matrix;
	int canvasXOffset;
	int canvasYOffset;
	float scaling;

	/**
	 * @param id
	 * @param scaleType
	 */
	public OneToOneScaling() {
		super(R.id.itemOneToOne,ScaleType.CENTER);
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
		canvasXOffset = -activity.vncCanvas.getCenteredXOffset();
		canvasYOffset = -activity.vncCanvas.getCenteredYOffset();
		activity.vncCanvas.computeShiftFromFullToView ();
		scaling = 1;
		activity.zoomer.setIsZoomOutEnabled(false);
		activity.zoomer.setIsZoomInEnabled(false);
		resetMatrix();
		matrix.postScale(scaling, scaling);
		activity.vncCanvas.setImageMatrix(matrix);
		resolveZoom(activity);
		activity.vncCanvas.pan(0, 0);
	}
}
