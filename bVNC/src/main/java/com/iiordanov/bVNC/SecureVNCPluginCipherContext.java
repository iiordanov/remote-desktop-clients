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

import org.bouncycastle.crypto.engines.AESEngine;
import org.bouncycastle.crypto.params.KeyParameter;

import java.io.IOException;

import javax.crypto.Cipher;

/**
 * Holds the cipher state for one direction of a SecureVNCPlugin connection.
 *
 * <p>Supports three modes:
 * <ul>
 *   <li>Single AES-OFB ({@code svncCipherAES}): one OFB keystream XORed with data.</li>
 *   <li>Single AES-CFB8 ({@code svncCipherAESCFB}): one CFB8 stream cipher.</li>
 *   <li>Triple AES-CFB8 ({@code svncCipher3AESOFB}): three CFB8 stages chained in sequence,
 *       with the middle stage direction swapped (matching UltraVNC's EDE-style construction).
 *       For sending (encryption): stage1(enc) → stage2(dec) → stage3(enc).
 *       For receiving (decryption): stage1 uses key3(dec) → stage2 uses key2(enc) → stage3 uses key1(dec)
 *       i.e. the key order is reversed and applied as the inverse operations.</li>
 * </ul>
 *
 * <p>Uses BouncyCastle's lightweight AES engine directly for reliable byte-granular processing.
 */
public class SecureVNCPluginCipherContext {

    private static final int BLOCK_SIZE = 16;

    /**
     * State for a single AES-OFB stream.
     */
    private static final class OFBStream {
        private final AESEngine aes = new AESEngine();
        private final byte[] feedback = new byte[BLOCK_SIZE];
        private final byte[] keystream = new byte[BLOCK_SIZE];
        private int pos = BLOCK_SIZE; // forces generation on first use

        OFBStream(byte[] key, byte[] iv) {
            aes.init(true, new KeyParameter(key));
            System.arraycopy(iv, 0, feedback, 0, BLOCK_SIZE);
        }

        byte next() {
            if (pos == BLOCK_SIZE) {
                aes.processBlock(feedback, 0, keystream, 0);
                System.arraycopy(keystream, 0, feedback, 0, BLOCK_SIZE);
                pos = 0;
            }
            return keystream[pos++];
        }
    }

    /**
     * State for a single AES-CFB8 stream.
     */
    private static final class CFB8Stream {
        private final AESEngine aes = new AESEngine();
        private final byte[] shiftRegister = new byte[BLOCK_SIZE];
        private final byte[] encBlock = new byte[BLOCK_SIZE];
        private final boolean forEncryption;

        CFB8Stream(byte[] key, byte[] iv, boolean forEncryption) {
            aes.init(true, new KeyParameter(key)); // CFB8 always uses AES-encrypt for keystream
            System.arraycopy(iv, 0, shiftRegister, 0, BLOCK_SIZE);
            this.forEncryption = forEncryption;
        }

        byte process(byte input) {
            aes.processBlock(shiftRegister, 0, encBlock, 0);
            byte output = (byte) (input ^ encBlock[0]);
            // Shift register left by one byte (drop byte 0), then append the new byte.
            for (int i = 0; i < BLOCK_SIZE - 1; i++) {
                shiftRegister[i] = shiftRegister[i + 1];
            }
            shiftRegister[BLOCK_SIZE - 1] = forEncryption ? output : input;
            return output;
        }
    }

    private final OFBStream ofb1;
    private final CFB8Stream cfb1; // single CFB8, or first stage of triple
    private final CFB8Stream cfb2; // null unless triple
    private final CFB8Stream cfb3; // null unless triple

    /**
     * Create a single-AES cipher context (OFB or CFB8).
     *
     * @param key     AES key bytes (16, 24, or 32 bytes)
     * @param iv      initial IV (16 bytes)
     * @param mode    {@link javax.crypto.Cipher#ENCRYPT_MODE} or {@link javax.crypto.Cipher#DECRYPT_MODE}
     * @param useCfb8 true for AES-CFB8, false for AES-OFB
     */
    public SecureVNCPluginCipherContext(byte[] key, byte[] iv, int mode, boolean useCfb8) {
        boolean forEnc = (mode == Cipher.ENCRYPT_MODE);
        if (useCfb8) {
            this.cfb1 = new CFB8Stream(key, iv, forEnc);
            this.ofb1 = null;
        } else {
            this.ofb1 = new OFBStream(key, iv);
            this.cfb1 = null;
        }
        this.cfb2 = null;
        this.cfb3 = null;
    }

    /**
     * Create a triple-AES CFB8 cipher context (svncCipher3AESOFB).
     *
     * <p>UltraVNC's 3AES mode chains three CFB8 stages with an EDE-style construction:
     * stage 2's direction is swapped relative to stages 1 and 3.
     *
     * <p>For encryption (C→S, OutStream): applies VS1(enc) → VS2(dec) → VS3(enc).
     * <p>For decryption (S→C, InStream): applies SV3(dec) → SV2(enc) → SV1(dec),
     * i.e. reverse key order and inverse directions.
     *
     * @param key1          key for stage 1 (sv1 or vs1)
     * @param key2          key for stage 2 (sv2 or vs2)
     * @param key3          key for stage 3 (sv3 or vs3)
     * @param iv            initial IV shared by all three stages (16 bytes)
     * @param forEncryption true for OutStream (C→S), false for InStream (S→C)
     */
    public SecureVNCPluginCipherContext(byte[] key1, byte[] key2, byte[] key3,
                                        byte[] iv, boolean forEncryption) {
        this.ofb1 = null;

        if (forEncryption) {
            // OutStream C→S: VS1(enc) → VS2(dec) → VS3(enc)
            this.cfb1 = new CFB8Stream(key1, iv, true);
            this.cfb2 = new CFB8Stream(key2, iv, false);
            this.cfb3 = new CFB8Stream(key3, iv, true);
        } else {
            // InStream S→C: inverse applied in reverse key order
            // Server sent: SV1(enc) → SV2(dec) → SV3(enc)
            // Client undoes: SV3_inv(dec) → SV2_inv(enc) → SV1_inv(dec)
            this.cfb1 = new CFB8Stream(key3, iv, false); // inverse of SV3 enc
            this.cfb2 = new CFB8Stream(key2, iv, true);  // inverse of SV2 dec
            this.cfb3 = new CFB8Stream(key1, iv, false); // inverse of SV1 enc
        }
    }

    /**
     * Process (encrypt or decrypt) bytes in-place.
     *
     * @param data   byte array to transform
     * @param offset start offset within data
     * @param length number of bytes to process
     * @throws IOException if cipher processing fails
     */
    public void process(byte[] data, int offset, int length) throws IOException {
        try {
            // Mode is determined by which streams the constructor created:
            // all three CFB8 stages = triple-AES; cfb1 only = single CFB8; ofb1 = OFB.
            if (cfb1 != null && cfb2 != null && cfb3 != null) {
                for (int i = 0; i < length; i++) {
                    byte b = cfb1.process(data[offset + i]);
                    b = cfb2.process(b);
                    data[offset + i] = cfb3.process(b);
                }
            } else if (cfb1 != null) {
                for (int i = 0; i < length; i++) {
                    data[offset + i] = cfb1.process(data[offset + i]);
                }
            } else if (ofb1 != null) {
                for (int i = 0; i < length; i++) {
                    data[offset + i] ^= ofb1.next();
                }
            } else {
                // Unreachable: a constructor always sets exactly one mode. Fail loud
                // rather than silently leaving the data unencrypted.
                throw new IllegalStateException("SecureVNCPlugin: no cipher configured");
            }
        } catch (Exception e) {
            throw new IOException("SecureVNCPlugin: cipher processing failed", e);
        }
    }

    /**
     * Process a single byte.
     *
     * @param b byte to process
     * @return processed byte
     * @throws IOException if cipher processing fails
     */
    public byte processByte(byte b) throws IOException {
        byte[] in = {b};
        process(in, 0, 1);
        return in[0];
    }
}
