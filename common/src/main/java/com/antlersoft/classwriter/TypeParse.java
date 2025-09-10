
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
package com.antlersoft.classwriter;

import java.util.ArrayList;
import java.util.Collections;

public class TypeParse
{
    public static final String ARG_ARRAYREF="A";
    public static final String ARG_BYTE="B";
    public static final String ARG_CHAR="C";
    public static final String ARG_DOUBLE="D";
    public static final String ARG_FLOAT="F";
    public static final String ARG_INT="I";
    public static final String ARG_LONG="J";
    public static final String ARG_OBJREF="L";
    public static final String ARG_SHORT="S";
    public static final String ARG_VOID="V";
    public static final String ARG_BOOLEAN="Z";

    static class ParseTable
    {
        ParseTable( char c1, String s1)
        {
            c=c1;
            s=s1;
        }
        char c;
        String s;
    }

    private static ParseTable table[]={
        new ParseTable( 'B', ARG_BYTE),
        new ParseTable( 'C', ARG_CHAR),
        new ParseTable( 'D', ARG_DOUBLE),
        new ParseTable( 'F', ARG_FLOAT),
        new ParseTable( 'I', ARG_INT),
        new ParseTable( 'J', ARG_LONG),
        new ParseTable( 'S', ARG_SHORT),
        new ParseTable( 'V', ARG_VOID),
        new ParseTable( 'Z', ARG_BOOLEAN)
        };

    private static ParseTable prefixes[]={
        new ParseTable( 'a', ARG_OBJREF),
        new ParseTable( 'a', ARG_ARRAYREF),
        new ParseTable( 'd', ARG_DOUBLE),
        new ParseTable( 'l', ARG_LONG),
        new ParseTable( 'f', ARG_FLOAT)
    };

    public static String parseFieldType( String simpleType)
    	throws CodeCheckException
    {
        return parseFieldArray( simpleType.toCharArray(),
        	new InstructionPointer( 0));
    }

    public static String getOpCodePrefix( String parsedType)
    {
        String result="i";

        for ( int i=0; i<prefixes.length; i++)
        {
            if ( prefixes[i].s==parsedType)
            {
                result=new String( new char[] { prefixes[i].c });
                break;
            }
        }

        return result;
    }

    public static String convertFromInternalClassName( String toConvert)
    {
		char[] nameBuffer=toConvert.toCharArray();
		for ( int i=0; i<nameBuffer.length; i++)
		{
		    if ( nameBuffer[i]=='$' || nameBuffer[i]=='/')
				nameBuffer[i]='.';
		}
		return new String( nameBuffer);
    }

    public static String packageFromInternalClassName( String toConvert)
    {
        int index=toConvert.lastIndexOf( '/');
        if ( index== -1)
            return new String();
        return convertFromInternalClassName( toConvert.substring( 0, index));
    }

	public static ArrayList<String> parseMethodType( String methodType)
 		throws CodeCheckException
 	{
  		char[] array=methodType.toCharArray();
    	try
     	{
          	ArrayList<String> result=new ArrayList<String>( 20);
	    	if ( array.length<3 || array[0]!='(')
	     	{
				throw new CodeCheckException( "Bad method signature "+methodType);
	   		}
	 		InstructionPointer offset=new InstructionPointer( 1);
	   		while ( array[offset.currentPos]!=')')
	      	{
				result.add( parseFieldArray( array, offset));
	        }
         	offset.currentPos++;
          	result.add( parseFieldArray( array, offset));

            Collections.reverse( result);
            return result;
         }
         catch ( ArrayIndexOutOfBoundsException bounds)
         {
             throw new CodeCheckException( "MethodType truncated");
         }
    }

    public static Object stackCategory( String arg_type)
    {
        if ( arg_type==ARG_VOID)
        	return null;
        if ( arg_type==ARG_DOUBLE || arg_type==ARG_LONG)
        	return ProcessStack.CAT2;
        return ProcessStack.CAT1;
    }

    private static String parseFieldArray( char[] array,
    	InstructionPointer offset)
     	throws CodeCheckException
    {
        try
        {
            if ( array[offset.currentPos]=='[')
            {
                while ( array[++offset.currentPos]=='[');
                parseBaseArray( array, offset);
                return ARG_ARRAYREF;
            }
            else
            	return parseBaseArray( array, offset);
        }
        catch ( ArrayIndexOutOfBoundsException bounds)
        {
            throw new CodeCheckException( "Type array truncated");
        }
    }

    private static String parseBaseArray( char[] array,
    	InstructionPointer offset)
     	throws CodeCheckException
    {
        if ( array[offset.currentPos]=='L')
        {
            while ( array[offset.currentPos++]!=';');
            return ARG_OBJREF;
        }
        else
        {
            for ( int i=0; i<table.length; i++)
            {
                if ( table[i].c==array[offset.currentPos])
                {
                    ++offset.currentPos;
                    return table[i].s;
                }
            }
        }
        throw new CodeCheckException( "Unknown character in type signature");
    }
}