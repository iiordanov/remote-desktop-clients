package com.keqisoft.android.spice.datagram;

public class MouseDG {
	private int dgType = DGType.ANDROID_UNKOWN;
	private int x;
	private int y;

	public MouseDG(int dgType, int x, int y) {
		this.dgType = dgType;
		this.x = x;
		this.y = y;
	}

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
}
