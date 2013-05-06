package com.keqisoft.android.spice.datagram;

import android.graphics.Bitmap;

public class BitmapDG {
	private int dgType = DGType.ANDROID_SHOW;
	private int w = 0;
	private int h = 0;
	private int x = 0;
	private int y = 0;
	private Bitmap bitmap;

	/**
	 * @return the dgType
	 */
	public int getDgType() {
		return dgType;
	}

	/**
	 * @param dgType
	 *            the dgType to set
	 */
	public void setDgType(int dgType) {
		this.dgType = dgType;
	}

	/**
	 * @return the w
	 */
	public int getW() {
		return w;
	}

	/**
	 * @param w
	 *            the w to set
	 */
	public void setW(int w) {
		if (this.w < w) {
			this.w = w;
		}
	}

	/**
	 * @return the h
	 */
	public int getH() {
		return h;
	}

	/**
	 * @param h
	 *            the h to set
	 */
	public void setH(int h) {
		if (this.h < h ) {
			this.h = h;
		}
	}

	/**
	 * @return the x
	 */
	public int getX() {
		return x;
	}

	/**
	 * @param x
	 *            the x to set
	 */
	public void setX(int x) {
		this.x = x;
	}

	/**
	 * @return the y
	 */
	public int getY() {
		return y;
	}

	/**
	 * @param y
	 *            the y to set
	 */
	public void setY(int y) {
		this.y = y;
	}

	/**
	 * @return the bitmap
	 */
	public Bitmap getBitmap() {
		return bitmap;
	}

	/**
	 * @param bitmap
	 *            the bitmap to set
	 */
	public void setBitmap(Bitmap bitmap) {
		this.bitmap = bitmap;
	}
}
