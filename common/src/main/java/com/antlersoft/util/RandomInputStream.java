
/**
 * Title:        antlersoft java software<p>
 * Description:  antlersoft Moose
 * antlersoft BBQ<p>
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

import java.io.IOException;
import java.io.InputStream;

public class RandomInputStream extends InputStream
{
	private static final int SIZE_INCREMENT=1024;

	public RandomInputStream()
	{
		position=0;
		count=0;
		buffer=new byte[SIZE_INCREMENT];
        mark_position= -1;
	}

	public synchronized void emptyAddBytes( byte[] toAdd, int offset,
        int length)
	{
		position=0;
        mark_position= -1;
		count=0;
		if ( length>buffer.length)
		{
			int newSize=( ( ( length)/SIZE_INCREMENT)+1)*SIZE_INCREMENT;
			byte[] newBuffer=new byte[newSize];
			buffer=newBuffer;
		}
		System.arraycopy( toAdd, offset, buffer, 0, length);
		count+=length;
	}

    public synchronized void emptyAddBytes( byte[] toAdd)
    {
        emptyAddBytes( toAdd, 0, toAdd.length);
    }
    
    public synchronized void addBytes( byte[] toAdd, int offset, int length)
    {
    	int new_length=count+length;
    	if ( new_length>buffer.length)
    	{
    		int newSize=((new_length/SIZE_INCREMENT)+1)*SIZE_INCREMENT;
    		byte[] newBuffer=new byte[newSize];
    		System.arraycopy( buffer, 0, newBuffer, 0, count);
    		buffer=newBuffer;
    	}
    	System.arraycopy( toAdd, offset, buffer, count, length);
    	count+=length;
    }

	private void packBuffer()
	{
		if ( position==count)
		{
			position=0;
			count=0;
            mark_position= -1;
		}
	}

	synchronized public int read()
		throws IOException
	{
		int retVal= -1;
		if ( position<count)
		{
			retVal=buffer[position++];
			if ( retVal<0)
				retVal+=256;
			packBuffer();
		}
		return retVal;
	}

	synchronized public int read( byte[] dest, int offset, int len)
		throws IOException
	{
		int retval= -1;
		if ( position<count)
		{
			if ( len>count-position)
				len=count-position;
			retval=len;
			System.arraycopy( buffer, position, dest, offset, len);
			position+=len;
			packBuffer();
		}
		return retval;
	}

	synchronized public long skip( long n)
		throws IOException
	{
		if ( n>count-position)
			n=count-position;
		position+=n;
		packBuffer();
		return n;
	}

	public int available()
	    throws IOException
	{
		return count-position;
	}

    public boolean markSupported()
    {
        return true;
    }

    public synchronized void mark( int mark_count)
    {
        mark_position=position;
    }

    public synchronized void reset()
        throws IOException
    {
        if ( mark_position== -1)
            throw new IOException( "No marked position");
        position=mark_position;
        mark_position= -1;
    }

	private int position;
	private int count;
    private int mark_position;
	private byte[] buffer;
}