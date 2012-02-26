/**
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

		activity.vncCanvas.absoluteXPosition = 0;
		activity.vncCanvas.absoluteYPosition = 0;
		activity.vncCanvas.scrollTo(0, 0);
		
		float ratioScreen = (float)activity.vncCanvas.getWidth()
                           /(float)activity.vncCanvas.getHeight();
		float ratioFrameBuffer = (float)activity.vncCanvas.rfb.framebufferWidth
                                /(float)activity.vncCanvas.rfb.framebufferHeight;

		// Compute the correct absoluteXPosition and absoluteYPosition values depending on the
		// height/width ratio of the device's screen vs. the framebuffer's ratio.
		if (ratioScreen > ratioFrameBuffer) {
			activity.vncCanvas.absoluteXPosition = 
					-((activity.vncCanvas.getWidth() - activity.vncCanvas.getHeight()
							*activity.vncCanvas.rfb.framebufferWidth
							/activity.vncCanvas.rfb.framebufferHeight));
			scaling = (float)activity.vncCanvas.getHeight()/(float)activity.vncCanvas.rfb.framebufferHeight;
		} else if (ratioScreen < ratioFrameBuffer) {
			activity.vncCanvas.absoluteYPosition =
					-((activity.vncCanvas.getHeight() - activity.vncCanvas.getWidth()
							*activity.vncCanvas.rfb.framebufferHeight
							/activity.vncCanvas.rfb.framebufferWidth));
			scaling = (float)activity.vncCanvas.getWidth()/(float)activity.vncCanvas.rfb.framebufferWidth;
		}
	}
}
