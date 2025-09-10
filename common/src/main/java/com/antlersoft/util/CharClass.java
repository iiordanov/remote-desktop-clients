/**
 * <p>Copyright (c) 2000-2007  Michael A. MacDonald<p>
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
 */
package com.antlersoft.util;

/**
 * @author Michael A. MacDonald
 *
 */
public class CharClass {
	public static final boolean isWhiteSpace( char c)
	{
		boolean result=false;
		switch ( c)
		{
		case ' ':
		case '\t':
		case '\r':
		case '\b':
		case '\n':
		case '\f':
			result=true;
		}
		return result;
	}

	public static final boolean isDigit( char c)
	{
		return c>='0' && c<='9';
	}

	public static final boolean isOctalDigit( char c)
	{
		return c>='0' && c<='7';
	}

	public static final boolean isHexDigit( char c)
	{
		return isDigit( c) || ( c>='A' && c<='F' ) || ( c>='a' && c<='f');
	}

	public static final boolean isIdentifierStart( char c)
	{
		return c=='_' || ( c>='a' && c<='z' ) || ( c>='A' && c<='Z');
	}

	public static final boolean isIdentifierPart( char c)
	{
		return isIdentifierStart( c) || isDigit( c);
	}

	public static final boolean isOperator( char c)
	{
		return ! ( isIdentifierPart(c) || c=='\'' || c=='"' || isWhiteSpace(c));
	}
}
