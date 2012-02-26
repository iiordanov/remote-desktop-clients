/**
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.widget.ImageView.ScaleType;

/**
 * @author Michael A. MacDonald
 */
class OneToOneScaling extends AbstractScaling {

	/**
	 * @param id
	 * @param scaleType
	 */
	public OneToOneScaling() {
		super(R.id.itemOneToOne,ScaleType.CENTER);
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
//		return mode == R.id.itemInputTouchPanZoomMouse;
		return true;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.bVNC.AbstractScaling#setScaleTypeForActivity(com.iiordanov.bVNC.VncCanvasActivity)
	 */
	@Override
	void setScaleTypeForActivity(VncCanvasActivity activity) {
		super.setScaleTypeForActivity(activity);
		activity.vncCanvas.scrollToAbsolute();
		activity.vncCanvas.pan(0,0);
	}

}
