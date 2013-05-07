package com.keqisoft.android.spice.datagram;

public class MouseDG {
	private int dgType = DGType.ANDROID_POINTER_EVENT;
	private int button;
	private int x;
	private int y;

	public MouseDG(int button, int x, int y) {
		this.button = button;
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
	 * @return the button
	 */
	public int getButton() {
		return button;
	}

	/**
	 * @param the button pressed/released
	 */
	public void setButton(int button) {
		this.button = button;
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
