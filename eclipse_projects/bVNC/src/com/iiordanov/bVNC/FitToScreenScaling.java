/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.util.Log;
import android.widget.ImageView.ScaleType;

/**
 * @author Michael A. MacDonald
 */
class FitToScreenScaling extends AbstractScaling {

	float scaling;

	/**
	 * @param id
	 * @param scaleType
	 */
	FitToScreenScaling() {
		super(R.id.itemFitToScreen, ScaleType.FIT_CENTER);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractScaling#getScale()
	 */
	@Override
	float getScale() {
		return scaling;
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
//		return mode == R.id.itemInputTouchpad;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractScaling#getDefaultHandlerId()
	 */
	@Override
	int getDefaultHandlerId() {
		return R.id.itemInputTouchpad;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractScaling#setCanvasScaleType(com.iiordanov.bVNC.VncCanvas)
	 */
	@Override
	void setScaleTypeForActivity(VncCanvasActivity activity) {
		super.setScaleTypeForActivity(activity);

		float factor = 1.f;
		activity.vncCanvas.absoluteXPosition = 0;
		activity.vncCanvas.absoluteYPosition = 0;
		activity.vncCanvas.scrollTo(0, 0);
		
		float screenWidth  = activity.vncCanvas.getWidth();
		float screenHeight = activity.vncCanvas.getHeight();
		float fbWidth  = activity.vncCanvas.rfb.framebufferWidth;
		float fbHeight = activity.vncCanvas.rfb.framebufferHeight;
		
		float ratioScreen = screenWidth/screenHeight;
		float ratioFrameBuffer = fbWidth/fbHeight;
		
		// Compute the correct absoluteXPosition and absoluteYPosition values depending on the
		// height/width ratio of the device's screen vs. the framebuffer's ratio.
		if (ratioScreen > ratioFrameBuffer) {
			if (fbWidth <= screenWidth)
				factor = ratioScreen;
				
			activity.vncCanvas.absoluteXPosition = -(int)(((screenWidth - screenHeight*fbWidth/fbHeight))/factor);
			scaling = screenHeight/fbHeight;
			
		} else if (ratioScreen < ratioFrameBuffer) {
			if (fbHeight <= screenHeight)
				factor = ratioScreen;
			
			activity.vncCanvas.absoluteYPosition = -(int)(((screenHeight - screenWidth*fbHeight/fbWidth))/factor);
			scaling = screenWidth/fbWidth;
		}
		
		Log.i("", "X position: " + activity.vncCanvas.absoluteXPosition
			    + " Y position: " + activity.vncCanvas.absoluteYPosition + " Scaling: " + scaling);
	}
}
