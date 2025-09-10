
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

import java.util.EmptyStackException;
import java.util.Stack;

public class DupX2Stack implements ProcessStack
{
    public DupX2Stack()
    {
    }

    public Stack stackUpdate(Stack old_stack) throws CodeCheckException
    {
        try
        {
	        Stack new_stack=(Stack)old_stack.clone();
	        Object top1=new_stack.pop();
	        if ( top1!=ProcessStack.CAT1)
	        	throw new CodeCheckException( "dup_x2; top object is not CAT1");
	        Object top2=new_stack.pop();
	        if ( top2==ProcessStack.CAT2)
	        {
	            new_stack.push( top1);
	            new_stack.push( top2);
	            new_stack.push( top1);
	        }
	        else
	        {
	            Object top3=new_stack.pop();
	            if ( top3!=ProcessStack.CAT1)
	            	throw new CodeCheckException( "dup_x2; third object is not CAT1");
	            new_stack.push( top1);
	            new_stack.push( top3);
                new_stack.push( top2);
                new_stack.push( top1);
	        }
         	return new_stack;
    	}
     	catch ( EmptyStackException ese)
      	{
             throw new CodeCheckException( "dup_x2; stack not deep enough");
        }
    }
}