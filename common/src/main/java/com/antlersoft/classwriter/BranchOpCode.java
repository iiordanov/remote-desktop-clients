
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

class BranchOpCode extends SimpleOpCode
{
	BranchOpCode( int v, int l, String m, int params)
 	{
  		super( v, l, m, new Cat1Stack( params, 0));
    }

    void traverse(Instruction instruction, Collection next,
    	CodeAttribute attribute)
    	throws com.antlersoft.classwriter.CodeCheckException
    {
        super.traverse( instruction, next, attribute);
        next.add( new InstructionPointer( instruction.getOffsetDestination()));
    }

    void fixDestinationAddress( Instruction instruction,
        int start, int oldPostEnd, int newPostEnd)
        throws CodeCheckException
    {
        instruction.fixDestinationAddress( start, oldPostEnd, newPostEnd);
    }
}