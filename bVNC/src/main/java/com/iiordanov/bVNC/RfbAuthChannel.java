/**
 * Copyright (C) 2026- Iordan Iordanov
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
 */

package com.iiordanov.bVNC;

import com.tigervnc.rdr.InStream;
import com.tigervnc.rdr.OutStream;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Channel exposing exactly what {@link UltraVncAuthenticator} needs from the
 * RFB connection — the wire I/O, the shared (generic-RFB) auth helpers, the
 * SecureVNCPlugin config, and the bits SecureVNCPlugin/MS-Logon require — without
 * the authenticator reaching into the rest of {@link RfbProto}.
 */
public interface RfbAuthChannel {
    InStream getInStream();

    OutStream getOutStream();

    void readSecurityResult(boolean userNameRequired, String authType) throws Exception;

    void readConnFailedReason() throws Exception;

    void authenticateVNC(String pw) throws Exception;

    void authenticateNone() throws Exception;

    SecureVncConfig getSecureVncConfig();

    InputStream getRawInputStream() throws IOException;

    OutputStream getRawOutputStream() throws IOException;

    /**
     * Set the encrypted streams back once the SecureVNCPlugin handshake produces them.
     */
    void setStreams(InStream is, OutStream os);

    /**
     * Ask the user whether to proceed without SecureVNCPlugin encryption (the
     * server offered it but it isn't enabled) and return their decision.
     */
    boolean confirmProceedWithoutEncryption();
}
