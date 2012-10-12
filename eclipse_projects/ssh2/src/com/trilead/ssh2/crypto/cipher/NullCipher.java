package com.trilead.ssh2.crypto.cipher;

/**
 * NullCipher.
 * 
 * @author Christian Plattner, plattner@trilead.com
 * @version $Id: NullCipher.java,v 1.1 2007/10/15 12:49:55 cplattne Exp $
 */
public class NullCipher implements BlockCipher {
	private int blockSize = 8;

	public NullCipher() {
	}

	public NullCipher(int blockSize) {
		this.blockSize = blockSize;
	}

	@Override
	public int getBlockSize() {
		return blockSize;
	}

	@Override
	public void init(boolean forEncryption, byte[] key) {
	}

	@Override
	public void transformBlock(byte[] src, int srcoff, byte[] dst, int dstoff) {
		System.arraycopy(src, srcoff, dst, dstoff, blockSize);
	}
}
