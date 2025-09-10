
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

import java.util.Collection;
import java.util.Stack;

public class SimpleOpCode extends OpCode
{
	private int length;
 	private ProcessStack stack_update;

	SimpleOpCode( int v, int l, String m, ProcessStack stacker)
	{
 		super( v, m);
	    length=l;
     	stack_update=stacker;
	}

 	SimpleOpCode( int v, int l, String m)
    {
    	this( v, l, m, new Cat1Stack( 1, 1));
    }

   	Stack stackUpdate( Instruction instruction, Stack current, CodeAttribute
    	attribute)
 		throws CodeCheckException
    {
        return stack_update.stackUpdate( current);
    }

    boolean isValidOperandLength( int len, boolean wide)
    {
        return len==length - 1;
    }

    final int getDefaultLength()
    {
        return length;
    }

    void traverse( Instruction instruction,	Collection next,
    	CodeAttribute attribute)
    	throws CodeCheckException
    {
        next.add(
        	new InstructionPointer( instruction.instructionStart
         		+instruction.getLength()));
    }

	Instruction read( InstructionPointer cr, byte[] code)
 		throws CodeCheckException
	{
      	if ( cr.wide)
         	throw new CodeCheckException( getMnemonic()+" can not be wide");

		Instruction result=new Instruction( this, cr.currentPos,
  			getSubArray( code, cr.currentPos+1, length-1), false);
	    cr.currentPos+=length;
     	return result;
	}
}