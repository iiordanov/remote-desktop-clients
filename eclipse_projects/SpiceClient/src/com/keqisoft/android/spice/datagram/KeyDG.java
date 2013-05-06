package com.keqisoft.android.spice.datagram;

public class KeyDG {
	private int dgType = DGType.ANDROID_UNKOWN;
	private int keycode;

	public KeyDG(int dgType, int keycode) {
		this.dgType = dgType;
		this.keycode = keycode;
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
	 * @return the keycode
	 */
	public int getKeycode() {
		return keycode;
	}

	/**
	 * @param keycode
	 *            the keycode to set
	 */
	public void setKeycode(int keycode) {
		this.keycode = keycode;
	}
}
