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

import com.tigervnc.rdr.InStream;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

/**
 * Decrypting InStream for SecureVNCPlugin.
 *
 * <p>Wraps a raw socket InputStream, transparently decrypting all incoming data
 * using the server-to-client {@link SecureVNCPluginCipherContext}.
 */
public class SecureVNCPluginInStream extends InStream {

    private final DataInputStream rawStream;
    private final SecureVNCPluginCipherContext cipher;

    /**
     * @param rawInputStream raw socket input stream (pre-encryption)
     * @param cipher         server-to-client cipher context (already initialized)
     */
    public SecureVNCPluginInStream(InputStream rawInputStream, SecureVNCPluginCipherContext cipher) {
        this.rawStream = new DataInputStream(rawInputStream);
        this.cipher = cipher;
    }

    @Override
    protected int overrun(int itemSize, int nItems, boolean wait) throws IOException {
        return itemSize * nItems;
    }

    @Override
    public int readByte() throws IOException {
        byte b = rawStream.readByte();
        return cipher.processByte(b);
    }

    @Override
    public int readUnsignedByte() throws IOException {
        return readByte() & 0xFF;
    }

    @Override
    public int readShort() throws IOException {
        byte[] buf = new byte[2];
        rawStream.readFully(buf);
        cipher.process(buf, 0, 2);
        return (buf[0] << 8) | (buf[1] & 0xFF);
    }

    @Override
    public int readUnsignedShort() throws IOException {
        return readShort() & 0xFFFF;
    }

    @Override
    public int readInt() throws IOException {
        byte[] buf = new byte[4];
        rawStream.readFully(buf);
        cipher.process(buf, 0, 4);
        return ((buf[0] & 0xFF) << 24) | ((buf[1] & 0xFF) << 16) | ((buf[2] & 0xFF) << 8) | (buf[3] & 0xFF);
    }

    @Override
    public int readUnsignedInt() throws IOException {
        return readInt();
    }

    @Override
    public long readLong() throws IOException {
        byte[] buf = new byte[8];
        rawStream.readFully(buf);
        cipher.process(buf, 0, 8);
        long result = 0;
        for (int i = 0; i < 8; i++) {
            result = (result << 8) | (buf[i] & 0xFF);
        }
        return result;
    }

    @Override
    public void readBytes(ByteBuffer data, int length) throws IOException {
        byte[] buf = new byte[length];
        rawStream.readFully(buf);
        cipher.process(buf, 0, length);
        data.put(buf);
    }

    @Override
    public void readBytes(byte[] bytes, int off, int length) throws IOException {
        rawStream.readFully(bytes, off, length);
        cipher.process(bytes, off, length);
    }

    @Override
    public void readBytes(byte[] bytes) throws IOException {
        readBytes(bytes, 0, bytes.length);
    }
}
