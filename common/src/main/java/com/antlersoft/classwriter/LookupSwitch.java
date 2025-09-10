
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

import com.antlersoft.util.NetByte;

class LookupSwitch extends SwitchOpCode
{
	LookupSwitch( int v, String m)
	{
	    super( v, m);
	}

	Stack stackUpdate( Instruction instruction, Stack old_stack, CodeAttribute
 		attribute)
   		throws CodeCheckException
    {
		Stack new_stack=(Stack)old_stack.clone();
  		if ( new_stack.size()<1)
    	{
         	throw new CodeCheckException( "Stack too small in lookupswitch");
        }
		if ( new_stack.pop()!=ProcessStack.CAT1)
  		{
        	throw new CodeCheckException( "lookupswitch: stack operand is wrong size");
        }
        return new_stack;
    }

    void traverse( Instruction instruction, Collection next, CodeAttribute
    	attribute)
     	throws CodeCheckException
    {
        int offset=instruction.operands.length%4;
        next.add( new InstructionPointer( NetByte.quadToInt( instruction.operands, offset)+
        	instruction.instructionStart));
        offset+=4;
        int npairs=NetByte.quadToInt( instruction.operands, offset);
        offset+=8;
        for ( int i=0; i<npairs; i++)
        {
            next.add( new InstructionPointer( NetByte.quadToInt( instruction.operands, offset+8*i)+
            	instruction.instructionStart));
        }
    }

    void fixDestinationAddress( Instruction instruction,
        int start, int oldPostEnd, int newPostEnd)
        throws CodeCheckException
    {
        int offset=instruction.operands.length%4;
        offset+=4;
        int npairs=NetByte.quadToInt( instruction.operands, offset);
        offset+=8;
        for ( int i=0; i<npairs; i++)
        {
            fixSwitchDestination( instruction, offset+8*i, oldPostEnd, newPostEnd);
        }
    }

	Instruction read( InstructionPointer cr, byte[] code)
 		throws CodeCheckException
	{
     	try
      	{
		    cr.currentPos++;
	     	int operandStart=cr.currentPos;
		    cr.currentPos+=(4-( cr.currentPos%4))%4;
		    cr.currentPos+=4;
		    int npairs=( NetByte.mU(code[cr.currentPos++])<<24)|
                (NetByte.mU(code[cr.currentPos++])<<16)|(NetByte.mU(code[cr.currentPos++])<<8)|NetByte.mU(code[cr.currentPos++]);
		    cr.currentPos+=8*npairs;
	     	return new Instruction( this, operandStart-1,
	      		getSubArray( code, operandStart,
	        	cr.currentPos-operandStart), false);
        }
        catch ( ArrayIndexOutOfBoundsException bounds)
        {
            throw new CodeCheckException( "Code segment too short for lookupswitch");
        }
	}
}