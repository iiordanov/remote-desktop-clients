
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

import com.antlersoft.util.NetByte;

public abstract class SwitchOpCode extends OpCode
{
    SwitchOpCode( int v, String m)
    {
        super( v, m);
    }

	Instruction repadInstruction( Instruction toRepad)
    throws CodeCheckException
    {
        int offset=toRepad.operands.length % 4;
        int new_offset=3 - ( toRepad.getInstructionStart() % 4);
        if ( offset!=new_offset)
        {
            byte[] new_operands=new byte[toRepad.operands.length+new_offset-offset];
            System.arraycopy( toRepad.operands, offset, new_operands, new_offset,
                              toRepad.operands.length - offset);
            NetByte.intToQuad( toRepad.operands.length, toRepad.operands, new_offset);
            Instruction result=new Instruction( this, toRepad.getInstructionStart(),
                                                new_operands, toRepad.wideFlag);
            fixDestinationAddress( result,
                toRepad.getInstructionStart()+toRepad.operands.length,
                toRepad.getInstructionStart()+toRepad.operands.length,
                toRepad.getInstructionStart()+toRepad.operands.length+new_offset-offset);
            return result;
        }
        else
            return null;
    }


    /**
     * Change one switch destination to match added or removed code
     */
    void fixSwitchDestination( Instruction instruction, int offset,
                               int oldPostEnd, int newPostEnd)
    {
        int old_dest=NetByte.quadToInt( instruction.operands, offset);
        if ( instruction.getInstructionStart()+old_dest>=oldPostEnd)
        {
            NetByte.intToQuad( old_dest+oldPostEnd-newPostEnd,
                               instruction.operands, offset);
        }
    }
}
