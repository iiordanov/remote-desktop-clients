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

import com.tigervnc.rdr.OutStream;

import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * Encrypting OutStream for SecureVNCPlugin.
 *
 * <p>Wraps a raw socket OutputStream, transparently encrypting all outgoing data
 * using the client-to-server {@link SecureVNCPluginCipherContext}.
 */
public class SecureVNCPluginOutStream extends OutStream {

    private final DataOutputStream rawStream;
    private final SecureVNCPluginCipherContext cipher;

    /**
     * @param rawOutputStream raw socket output stream (pre-encryption)
     * @param cipher          client-to-server cipher context (already initialized)
     */
    public SecureVNCPluginOutStream(OutputStream rawOutputStream, SecureVNCPluginCipherContext cipher) {
        this.rawStream = new DataOutputStream(rawOutputStream);
        this.cipher = cipher;
    }

    @Override
    public int length() {
        return 0;
    }

    @Override
    protected int overrun(int itemSize, int nItems) throws IOException {
        return itemSize * nItems;
    }

    @Override
    public void writeU8(int u) throws IOException {
        byte[] buf = {(byte) u};
        cipher.process(buf, 0, 1);
        rawStream.write(buf[0]);
    }

    @Override
    public void writeU16(int u) throws IOException {
        byte[] buf = {(byte) (u >> 8), (byte) u};
        cipher.process(buf, 0, 2);
        rawStream.write(buf);
    }

    @Override
    public void writeU32(int u) throws IOException {
        byte[] buf = {(byte) (u >> 24), (byte) (u >> 16), (byte) (u >> 8), (byte) u};
        cipher.process(buf, 0, 4);
        rawStream.write(buf);
    }

    @Override
    public void writeBytes(byte[] data, int dataPtr, int length) throws IOException {
        byte[] buf = new byte[length];
        System.arraycopy(data, dataPtr, buf, 0, length);
        cipher.process(buf, 0, length);
        rawStream.write(buf, 0, length);
    }

    @Override
    public void writeBytes(byte[] data) throws IOException {
        writeBytes(data, 0, data.length);
    }

    @Override
    public void flush() throws IOException {
        rawStream.flush();
    }
}
