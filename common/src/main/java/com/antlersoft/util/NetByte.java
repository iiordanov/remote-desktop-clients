
/**
 * Title:        <p>
 * Description:  Java object database; also code analysis tool<p>
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
 * Company:      <p>
 * @author Michael MacDonald
 * @version 1.0
 */

package com.antlersoft.util;

// java packages

// local packages

/**
 * Some methods for dealing with byte arrays that contain integral values
 * in network byte-order, as you might obtain from network communication
 * protocols or Java class files.
 *
 * @version    $Revision: 1.3 $
 * @author     Michael MacDonald
 */

public final class NetByte
{

    //-------------------------------------------------------------------
    // attributes
    //-------------------------------------------------------------------
    public static final String _ID_ = "$Id: NetByte.java,v 1.3 2005/05/31 18:09:07 mike Exp $";

    //-------------------------------------------------------------------
    // public interface
    //-------------------------------------------------------------------

    /**
     * Makes an unsigned int value out of a byte
     *
     * @param b    byte value to make unsigned
     */
    public static final int mU( byte b)
    {
		int retval=b;
		if ( retval<0)
		{
		    retval+=256;
		}
		return retval;
    }

    /**
     * Writes the least significant 16 bits of an int to a pair of bytes,
     * in network byte order.
     *
     * @param value    Integer value to write into the byte array
     * @param array    The byte array to receive the value
     * @param offset   Position within the byte array that the first byte
     * of the value should be written
     */
    public static final void intToPair( int value, byte[] array, int offset)
    {
        array[offset+1]=(byte)(value&0xff);
        value>>>=8;
        array[offset]=(byte)value;
    }

    /**
     * Writes an integer to four bytes, in network byte order.
     *
     * @param value    Integer value to write into the byte array
     * @param array    The byte array to receive the value
     * @param offset   Position within the byte array that the first byte
     * of the value should be written
     */
    public static final void intToQuad( int value, byte[] array, int offset)
    {
        array[offset+3]=(byte)(value&0xff);
        value>>>=8;
        array[offset+2]=(byte)(value&0xff);
        value>>>=8;
        array[offset+1]=(byte)(value&0xff);
        value>>>=8;
        array[offset]=(byte)value;
    }

    /**
     * Interprets a pair of bytes as a 16-bit integer in network byte order.
     *
     * @param array    Byte array that contains bytes to be interpreted as
     * integer value
     * @param offset   Position within array of the first byte to be interpreted
     */
    public static final int pairToInt( byte[] array, int offset)
    {
        return (array[offset]<<8)|mU( array[offset+1]);
    }

    /**
     * Interprets a sequence of four bytes as an integer in network byte order.
     *
     * @param array    Byte array that contains bytes to be interpreted as
     * integer value
     * @param offset   Position within array of the first byte to be interpreted
     */
    public static final int quadToInt( byte[] array, int offset)
    {
        return (array[offset]<<24)|( mU( array[offset+1])<<16)|
        	( mU( array[offset+2])<<8)|mU( array[offset+3]);
    }
}
