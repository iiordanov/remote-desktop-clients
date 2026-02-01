/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2011 Brian P. Hinz
 * Copyright (C) 2022 D. R. Commander.  All Rights Reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.tigervnc.rdr;

import java.io.IOException;
import java.util.zip.Deflater;

public class ZlibOutStream extends OutStream {

    static final int DEFAULT_BUF_SIZE = 16384;

    public ZlibOutStream(OutStream os, int bufSize_, int compressLevel) {
        underlying = os;
        compressionLevel = newLevel = compressLevel;
        bufSize = (bufSize_ != 0 ? bufSize_ : DEFAULT_BUF_SIZE);

        deflater = new Deflater(compressLevel);

        b = new byte[bufSize];
        ptr = 0;
        end = bufSize;
    }

    public ZlibOutStream() {
        this(null, 0, Deflater.DEFAULT_COMPRESSION);
    }

    public void close() {
        b = null;
        if (deflater != null) {
            deflater.end();
        }
    }

    public void setUnderlying(OutStream os) {
        underlying = os;
    }

    public void setCompressionLevel(int level) {
        if (level < -1 || level > 9)
            level = Deflater.DEFAULT_COMPRESSION;

        newLevel = level;
    }

    public int length() {
        return offset + ptr;
    }

    public void flush() throws IOException {
        checkCompressionLevel();

        deflater.setInput(b, 0, ptr);

        // Force out everything from the deflater
        deflate(Deflater.SYNC_FLUSH);

        offset += ptr;
        ptr = 0;
    }

    public int overrun(int itemSize, int nItems) throws IOException {
        if (itemSize > bufSize)
            throw new IOException("ZlibOutStream overrun: max itemSize exceeded");

        checkCompressionLevel();

        while ((end - ptr) < itemSize) {
            deflater.setInput(b, 0, ptr);

            deflate(Deflater.NO_FLUSH);

            // output buffer not full

            offset += ptr;
            ptr = 0;
        }

        int nAvail = (end - ptr) / itemSize;
        return Math.min(nAvail, nItems);

    }

    void deflate(int flushMode) throws IOException {
        if (underlying == null)
            throw new IOException("ZlibOutStream: underlying OutStream has not been set");

        if ((flushMode == Deflater.NO_FLUSH) && deflater.needsInput())
            return;

        byte[] outBuf = new byte[bufSize];

        do {
            underlying.check(1);
            int n = deflater.deflate(outBuf, 0, bufSize, flushMode);
            if (n > 0) {
                underlying.writeBytes(outBuf, 0, n);
            }

            if (deflater.needsInput() && flushMode != Deflater.NO_FLUSH)
                break;

        } while (!deflater.finished());
    }

    public void checkCompressionLevel() {
        if (newLevel != compressionLevel) {
            // Java Deflater requires finishing current deflate before changing level
            deflater.finish();

            deflater = new Deflater(newLevel);
            compressionLevel = newLevel;
        }
    }

    private OutStream underlying;
    private int compressionLevel;
    private int newLevel;
    private final int bufSize;
    private int offset;
    private Deflater deflater;
}
