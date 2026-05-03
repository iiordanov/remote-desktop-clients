/**
 * Copyright (C) 2026 Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * <p>
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <p>
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC;

import android.util.Base64;
import android.util.Log;

import org.bouncycastle.asn1.ASN1Integer;
import org.bouncycastle.asn1.ASN1Sequence;
import org.bouncycastle.asn1.DERNull;
import org.bouncycastle.asn1.oiw.OIWObjectIdentifiers;
import org.bouncycastle.asn1.pkcs.RSAPrivateKey;
import org.bouncycastle.asn1.pkcs.RSAPublicKey;
import org.bouncycastle.asn1.x509.AlgorithmIdentifier;
import org.bouncycastle.asn1.x509.DigestInfo;

import java.io.IOException;
import java.math.BigInteger;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.Signature;
import java.security.interfaces.RSAPrivateCrtKey;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.RSAPrivateCrtKeySpec;

/**
 * Produces a SHA1withRSA signature for SecureVNCPlugin client authentication.
 *
 * <p>UltraVNC uses PKCS#1 RSAPrivateKey DER format for client auth keys (.pkey files),
 * and OpenSSL's RSA_sign(NID_sha1, ...) which takes a pre-computed SHA-1 digest and
 * wraps it in DigestInfo before RSA-signing (no re-hashing).
 *
 * <p>The signed data is: SHA-1(serverRsaPubKeyBytes || sessionKeyMaterial)
 */
public final class SecureVNCPluginClientAuth {
    private static final String TAG = "SVNCClientAuth";

    private SecureVNCPluginClientAuth() {
    }

    /**
     * Sign the authentication payload using the client's RSA private key.
     *
     * <p>The signature covers: SHA-1(serverPublicKeyRaw || sessionKeyMaterial).
     * Uses NONEwithRSA to avoid double-hashing, matching UltraVNC's RSA_sign(NID_sha1, digest, ...).
     *
     * @param serverRsaPubKeyBytes raw plaintext PKCS#1 RSA public key bytes from the challenge
     * @param sessionKeyMaterial   random key material used in the response
     * @param privateKeyBase64     base64-encoded private key (PKCS#1 or PKCS#8 DER format).
     *                             Only unencrypted keys are supported (UltraVNC generates
     *                             client keys without a passphrase).
     * @return PKCS#1 v1.5 RSA signature bytes
     * @throws IOException if signing fails
     */
    public static byte[] sign(byte[] serverRsaPubKeyBytes, byte[] sessionKeyMaterial,
                              String privateKeyBase64) throws IOException {
        try {
            // Compute SHA-1(serverPublicKeyRaw || sessionKeyMaterial)
            MessageDigest sha1 = MessageDigest.getInstance("SHA-1");
            sha1.update(serverRsaPubKeyBytes);
            sha1.update(sessionKeyMaterial);
            byte[] digest = sha1.digest();

            // Build DigestInfo(SHA-1, digest) to match OpenSSL's RSA_sign(NID_sha1, ...)
            AlgorithmIdentifier sha1AlgId = new AlgorithmIdentifier(OIWObjectIdentifiers.idSHA1, DERNull.INSTANCE);
            byte[] digestInfo = new DigestInfo(sha1AlgId, digest).getEncoded();

            PrivateKey privateKey = loadPrivateKey(privateKeyBase64);

            // Use NONEwithRSA: raw PKCS#1 v1.5 signing without any additional hashing.
            // We pass the complete DigestInfo, matching what OpenSSL's RSA_sign does.
            Signature signer = Signature.getInstance("NONEwithRSA");
            signer.initSign(privateKey);
            signer.update(digestInfo);
            return signer.sign();
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            throw new IOException("SecureVNCPlugin: client auth signing failed", e);
        }
    }

    /**
     * Load a private key from its base64-encoded DER representation.
     * Tries PKCS#1 RSAPrivateKey format first (UltraVNC .pkey files),
     * then falls back to PKCS#8 PrivateKeyInfo format (Java/SSH style keys).
     *
     * @param base64Key base64-encoded private key DER bytes
     * @return PrivateKey instance
     * @throws IOException if key loading fails in both formats
     */
    private static PrivateKey loadPrivateKey(String base64Key) throws IOException {
        try {
            byte[] keyBytes = Base64.decode(base64Key, Base64.DEFAULT);

            Log.i(TAG, "Loading private key, DER length=" + keyBytes.length);

            // Try PKCS#1 RSAPrivateKey first (UltraVNC format)
            try {
                PrivateKey key = loadPkcs1PrivateKey(keyBytes);
                Log.i(TAG, "Loaded PKCS#1 RSAPrivateKey (UltraVNC format)");
                return key;
            } catch (Exception e) {
                Log.d(TAG, "Not PKCS#1 format, trying PKCS#8: " + e.getMessage());
            }

            // Fall back to PKCS#8 PrivateKeyInfo (Java/SSH format)
            PKCS8EncodedKeySpec spec = new PKCS8EncodedKeySpec(keyBytes);
            KeyFactory kf = KeyFactory.getInstance("RSA");
            PrivateKey key = kf.generatePrivate(spec);
            Log.i(TAG, "Loaded PKCS#8 PrivateKeyInfo (Java/SSH format)");
            return key;
        } catch (Exception e) {
            throw new IOException("SecureVNCPlugin: failed to load client private key", e);
        }
    }

    /**
     * Parse a PKCS#1 RSAPrivateKey DER structure into a Java PrivateKey.
     *
     * <p>PKCS#1 RSAPrivateKey ASN.1 structure (RFC 3447):
     * <pre>
     * RSAPrivateKey ::= SEQUENCE {
     *   version           INTEGER,
     *   modulus           INTEGER,  -- n
     *   publicExponent    INTEGER,  -- e
     *   privateExponent   INTEGER,  -- d
     *   prime1            INTEGER,  -- p
     *   prime2            INTEGER,  -- q
     *   exponent1         INTEGER,  -- d mod (p-1)
     *   exponent2         INTEGER,  -- d mod (q-1)
     *   coefficient       INTEGER   -- (inverse of q) mod p
     * }
     * </pre>
     */
    private static PrivateKey loadPkcs1PrivateKey(byte[] derBytes) throws Exception {
        ASN1Sequence seq = ASN1Sequence.getInstance(derBytes);
        if (seq.size() < 9) {
            throw new IllegalArgumentException("Not a valid PKCS#1 RSAPrivateKey (need 9 integers, got " + seq.size() + ")");
        }

        BigInteger modulus = ASN1Integer.getInstance(seq.getObjectAt(1)).getValue();
        BigInteger publicExponent = ASN1Integer.getInstance(seq.getObjectAt(2)).getValue();
        BigInteger privateExponent = ASN1Integer.getInstance(seq.getObjectAt(3)).getValue();
        BigInteger primeP = ASN1Integer.getInstance(seq.getObjectAt(4)).getValue();
        BigInteger primeQ = ASN1Integer.getInstance(seq.getObjectAt(5)).getValue();
        BigInteger exponentP = ASN1Integer.getInstance(seq.getObjectAt(6)).getValue();
        BigInteger exponentQ = ASN1Integer.getInstance(seq.getObjectAt(7)).getValue();
        BigInteger crtCoefficient = ASN1Integer.getInstance(seq.getObjectAt(8)).getValue();

        RSAPrivateCrtKeySpec spec = new RSAPrivateCrtKeySpec(
                modulus, publicExponent, privateExponent,
                primeP, primeQ, exponentP, exponentQ, crtCoefficient);

        KeyFactory kf = KeyFactory.getInstance("RSA");
        return kf.generatePrivate(spec);
    }

    /**
     * Generate an RSA keypair for SecureVNCPlugin client authentication.
     * Keys are encoded in PKCS#1 DER format (matching UltraVNC's .pkey/.pubkey files).
     *
     * @param keyBits RSA key size in bits (e.g. 1024, 2048, 3072)
     * @return GeneratedKeyPair with base64-encoded private key and raw DER public key bytes
     * @throws IOException if key generation fails
     */
    public static GeneratedKeyPair generateKeyPair(int keyBits) throws IOException {
        try {
            KeyPairGenerator kpg = KeyPairGenerator.getInstance("RSA");
            kpg.initialize(keyBits);
            KeyPair pair = kpg.generateKeyPair();

            RSAPrivateCrtKey privKey = (RSAPrivateCrtKey) pair.getPrivate();
            java.security.interfaces.RSAPublicKey pubKey =
                    (java.security.interfaces.RSAPublicKey) pair.getPublic();

            // Encode private key as PKCS#1 RSAPrivateKey DER (matching UltraVNC's i2d_RSAPrivateKey)
            RSAPrivateKey pkcs1Priv = new RSAPrivateKey(
                    privKey.getModulus(), privKey.getPublicExponent(),
                    privKey.getPrivateExponent(),
                    privKey.getPrimeP(), privKey.getPrimeQ(),
                    privKey.getPrimeExponentP(), privKey.getPrimeExponentQ(),
                    privKey.getCrtCoefficient());
            byte[] privDer = pkcs1Priv.getEncoded();

            // Encode public key as PKCS#1 RSAPublicKey DER (matching UltraVNC's i2d_RSAPublicKey)
            RSAPublicKey pkcs1Pub = new RSAPublicKey(
                    pubKey.getModulus(), pubKey.getPublicExponent());
            byte[] pubDer = pkcs1Pub.getEncoded();

            String privBase64 = Base64.encodeToString(privDer, Base64.DEFAULT);
            return new GeneratedKeyPair(privBase64, pubDer);
        } catch (Exception e) {
            throw new IOException("SecureVNCPlugin: key generation failed", e);
        }
    }

    /**
     * Holds the results of client auth key generation.
     *
     * @param privateKeyBase64 Base64-encoded PKCS#1 RSAPrivateKey DER (for storage in connection settings).
     * @param publicKeyDer     Raw PKCS#1 RSAPublicKey DER bytes (for export as .pubkey file to the server).
     */
    public record GeneratedKeyPair(String privateKeyBase64, byte[] publicKeyDer) {
    }
}
