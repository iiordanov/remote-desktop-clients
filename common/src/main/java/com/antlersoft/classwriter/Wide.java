
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

/**
 *  Needs no special processing (other than SimpleOpCode) but a separate
 *  class to mark that next opcode made need special handling
 */
public class Wide extends SimpleOpCode
{
	Wide( int v, String m)
 	{
        super( v, 1, m, new Cat1Stack( 0, 0));
    }

    void traverse( Instruction instruction,	Collection next)
		throws CodeCheckException
    {
        InstructionPointer ip=
        	new InstructionPointer( instruction.instructionStart
         		+instruction.getLength());
        ip.wide=true;
        next.add( ip);
    }
}