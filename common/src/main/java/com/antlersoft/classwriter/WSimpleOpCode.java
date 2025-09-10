
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

public class WSimpleOpCode extends SimpleOpCode
{
	WSimpleOpCode( int v, int l, String m, ProcessStack stacker)
	{
 		super( v, l, m, stacker);
	}

 	WSimpleOpCode( int v, int l, String m)
    {
    	super( v, l, m);
    }

    boolean isValidOperandLength( int len, boolean wide)
    {
        int operand_length=getDefaultLength()-1;
        if ( wide)
            operand_length*=2;
        return len==operand_length;
    }

	Instruction read( InstructionPointer cr, byte[] code)
 		throws CodeCheckException
	{
     	int wideLength;
      	if ( cr.wide)
         	wideLength=2*(getDefaultLength()-1)+1;
        else
            wideLength=getDefaultLength();
		Instruction result=new Instruction( this, cr.currentPos,
  			getSubArray( code, cr.currentPos+1, wideLength-1), cr.wide);
	    cr.currentPos+=wideLength;
     	return result;
	}
}