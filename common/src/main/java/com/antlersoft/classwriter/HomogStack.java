
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

import java.util.Stack;

class HomogStack implements ProcessStack
{
	private Object entry_type;
 	private int pops;
  	private int pushes;

    HomogStack( Object type, int pop, int push)
    {
        entry_type=type;
        pops=pop;
        pushes=push;
    }

   	public Stack stackUpdate( Stack old_stack)
 		throws CodeCheckException
   	{
        if ( old_stack.size()<pops)
        {
        	throw new CodeCheckException( "Not enough entries on the stack");
        }
        Stack new_stack=(Stack)old_stack.clone();
        int i;
        for ( i=0; i<pops; i++)
        {
            if ( new_stack.pop()!=entry_type)
            {
                throw new CodeCheckException( "Invalid stack entry type");
            }
        }
        for ( i=0; i<pushes; i++)
        {
            new_stack.push( entry_type);
        }

        return new_stack;
	}
}