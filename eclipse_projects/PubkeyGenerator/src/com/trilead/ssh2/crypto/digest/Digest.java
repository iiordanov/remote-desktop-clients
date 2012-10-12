package com.trilead.ssh2.crypto.digest;

/**
 * Digest.
 * 
 * @author Christian Plattner, plattner@trilead.com
 * @version $Id: Digest.java,v 1.1 2007/10/15 12:49:57 cplattne Exp $
 */
public interface Digest {
	public void digest(byte[] out);

	public void digest(byte[] out, int off);

	public int getDigestLength();

	public void reset();

	public void update(byte b);

	public void update(byte b[], int off, int len);

	public void update(byte[] b);
}
