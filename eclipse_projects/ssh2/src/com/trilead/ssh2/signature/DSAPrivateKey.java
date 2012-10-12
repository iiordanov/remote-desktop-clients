package com.trilead.ssh2.signature;

import java.math.BigInteger;

/**
 * DSAPrivateKey.
 * 
 * @author Christian Plattner, plattner@trilead.com
 * @version $Id: DSAPrivateKey.java,v 1.1 2007/10/15 12:49:57 cplattne Exp $
 */
public class DSAPrivateKey {
	private BigInteger p;
	private BigInteger q;
	private BigInteger g;
	private BigInteger x;
	private BigInteger y;

	public DSAPrivateKey(BigInteger p, BigInteger q, BigInteger g,
			BigInteger y, BigInteger x) {
		this.p = p;
		this.q = q;
		this.g = g;
		this.y = y;
		this.x = x;
	}

	public BigInteger getG() {
		return g;
	}

	public BigInteger getP() {
		return p;
	}

	public DSAPublicKey getPublicKey() {
		return new DSAPublicKey(p, q, g, y);
	}

	public BigInteger getQ() {
		return q;
	}

	public BigInteger getX() {
		return x;
	}

	public BigInteger getY() {
		return y;
	}
}