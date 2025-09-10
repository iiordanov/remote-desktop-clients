
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

class ConvertStack implements ProcessStack
{
	private Object from_type;
 	private Object to_type;

	ConvertStack( Object from, Object to)
 	{
		from_type=from;
  		to_type=to;
    }

   	public Stack stackUpdate( Stack old_stack)
 		throws CodeCheckException
   	{
        if ( old_stack.size()<1)
        {
        	throw new CodeCheckException( "One entry on the stack required to convert");
        }
        Stack new_stack=(Stack)old_stack.clone();
        if ( new_stack.pop()!=from_type)
 	       throw new CodeCheckException( "Invalid stack entry type to convert");

    	new_stack.push( to_type);
        return new_stack;
	}
}