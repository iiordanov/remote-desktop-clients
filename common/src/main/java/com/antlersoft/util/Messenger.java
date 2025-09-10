
/**
 * Title:        Remote File System for OpenTools<p>
 * Description:  An OpenTools vfs filesystem for accessing files on a remote host<p>
 * <p>Copyright (c) 2000-2005  Michael A. MacDonald<p>
 * ----- - - -- - - --
 * <p>
 *     This package is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * <p>
 *     This package is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * <p>
 *     You should have received a copy of the GNU General Public License
 *     along with the package (see gpl.txt); if not, see www.gnu.org
 * <p>
 * ----- - - -- - - --
 * Company:      antlersoft<p>
 * @author Michael MacDonald
 * @version 1.0
 */
package com.antlersoft.util;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInput;
import java.io.DataInputStream;
import java.io.DataOutput;
import java.io.DataOutputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStreamWriter;

import java.net.Socket;

public class Messenger
{
    public Messenger( Socket sock)
        throws IOException
    {
        socket=sock;
        socketInput=new DataInputStream( new BufferedInputStream(
            socket.getInputStream()));
        socketOutput=new DataOutputStream( new BufferedOutputStream(
            socket.getOutputStream()));
        randomInput=new RandomInputStream();
        randomOutput=new RandomOutputStream();
        outMessage=new DataOutputStream( randomOutput);
        inMessage=new DataInputStream( randomInput);
        defaultBuffer=new byte[DEFAULT_BUFFER_LENGTH];
    }

    public DataInput getDataInput()
    {
        return inMessage;
    }

    public DataOutput getDataOutput()
    {
        return outMessage;
    }

    public void sendMessage()
        throws IOException
    {
        synchronized ( outLock)
        {
            outMessage.flush();
            byte[] outBuffer=randomOutput.getWrittenBytes();
            socketOutput.writeInt( outBuffer.length);
            socketOutput.write( outBuffer);
            socketOutput.flush();
        }
    }

    public void receiveMessage()
        throws IOException
    {
        synchronized ( inLock)
        {
            int messageLength=socketInput.readInt();
            byte[] inBuffer;
            if ( messageLength<=DEFAULT_BUFFER_LENGTH)
                inBuffer=defaultBuffer;
            else
                inBuffer=new byte[messageLength];
            socketInput.readFully( inBuffer, 0, messageLength);
            randomInput.emptyAddBytes( inBuffer);
        }
    }

    public void clearOutputMessage()
        throws IOException
    {
        synchronized( outLock)
        {
            outMessage.flush();
            randomOutput.getWrittenBytes();
        }
    }

    public void sendRequest()
        throws IOException
    {
        sendMessage();
        receiveMessage();
    }

    public void close()
        throws IOException
    {
        synchronized ( outLock)
        {
            synchronized( inLock)
            {
                socket.close();
                socket=null;
                socketInput=null;
                socketOutput=null;
                randomInput=null;
                randomOutput=null;
                inMessage=null;
                outMessage=null;
            }
        }
    }

    public String inputString()
    throws IOException
	{
	    int l=getDataInput().readInt();
	    char[] buf=new char[l];
	    new InputStreamReader( (DataInputStream)getDataInput(), "UTF-8").read(buf, 0, l);
	    return new String( buf);
	}
    
    public void writeString( String to_write)
    throws IOException
    {
    	getDataOutput().writeInt( to_write.length());
    	new OutputStreamWriter( (DataOutputStream)getDataOutput(), "UTF-8").append(to_write).flush();
    }
    
    private final int DEFAULT_BUFFER_LENGTH=10240;
    private byte[] defaultBuffer;
    private Socket socket;
    private DataInputStream socketInput;
    private DataOutputStream socketOutput;
    private RandomInputStream randomInput;
    private RandomOutputStream randomOutput;
    private DataOutputStream outMessage;
    private DataInputStream inMessage;
    private Object inLock=new Object();
    private Object outLock=new Object();
}
