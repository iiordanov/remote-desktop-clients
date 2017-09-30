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


package com.undatech.opaque;

import com.undatech.opaque.R;

import android.graphics.Matrix;

public class CanvasZoomer {
	static final String TAG = "CanvasZoomer";

	RemoteCanvas canvas;
	float minimumZoom;
	float maximumZoom;
	private Matrix matrix;
	float zoomFactor;
	
	public CanvasZoomer(RemoteCanvas canvas) {
		this.canvas = canvas;
		matrix = new Matrix();
		zoomFactor = 1;
		maximumZoom = 4;
	}
	
	public float getZoomFactor() {
		return zoomFactor;
	}
	
	public void resetScaling () {
		minimumZoom = canvas.getMinimumScale();
		if (minimumZoom > 1.f) {
            zoomFactor = minimumZoom;
		} else {
		    zoomFactor = 1.f;
		}
		reinitCanvasMatrix();
		canvas.resetScroll();
	}
	
	private void reinitCanvasMatrix() {
		canvas.computeShiftFromFullToView ();
		matrix.reset();
		matrix.preTranslate(-canvas.getShiftX(), -canvas.getShiftY());
		matrix.postScale(zoomFactor, zoomFactor);
		canvas.setImageMatrix(matrix);
	}
	
	public boolean changeZoom(float dZ) {	
		float newZoomFactor = zoomFactor * dZ;
		
		// Ensure we're not going above or below the max and min zoom limits.
		if (dZ < 1) {
			if (newZoomFactor < minimumZoom) newZoomFactor = minimumZoom;
		} else {
			if (newZoomFactor > maximumZoom) newZoomFactor = maximumZoom;
		}
		
		// Here we do snapping to 1:1. If we are approaching scale = 1, we snap to it.
		float oldZoomFactor = zoomFactor;
		if ( (newZoomFactor > 0.95f && newZoomFactor < 1.00f) ||
			 (newZoomFactor > 1.00f && newZoomFactor < 1.05f) ) {
			newZoomFactor = 1.f;
			// Only if oldZoomFactor is outside the snap region, do we inform the user.
			if (oldZoomFactor < 0.95f || oldZoomFactor > 1.05f)
				canvas.displayShortToastMessage(R.string.snap_one_to_one);
		}
		zoomFactor = newZoomFactor;
		reinitCanvasMatrix();
		
		// Only if we have actually scaled do we pan and potentially set mouse position.
		if (oldZoomFactor != newZoomFactor) {
			return true;
		} else {
			return false;
		}
	}
}
