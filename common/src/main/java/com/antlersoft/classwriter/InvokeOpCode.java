
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
import java.util.Iterator;
import java.util.ArrayList;

public class InvokeOpCode extends SimpleOpCode
{
	InvokeOpCode( int v, int l, String m)
	{
		super( v, l, m, null);
	}

    Stack stackUpdate(Instruction instruction, Stack old_stack,
    	CodeAttribute attribute)
    	throws CodeCheckException
    {
        Stack new_stack=(Stack)old_stack.clone();
        ClassWriter.CPTypeRef typeRef=instruction.getSymbolicReference(
        	attribute.getCurrentClass());
        ArrayList arg_list=TypeParse.parseMethodType( typeRef.getSymbolType());
        Iterator i=arg_list.iterator();
        Object return_stack=TypeParse.stackCategory( (String)i.next());
        while ( i.hasNext())
        {
            Object popped=new_stack.pop();
            if ( popped!=TypeParse.stackCategory( (String)i.next()))
            	throw new CodeCheckException( "Bad invocation arguments");
        }
        if ( ! getMnemonic().equals( "invokestatic"))
        {
            Object popped=new_stack.pop();
            if ( popped!=ProcessStack.CAT1)
            	throw new
             		CodeCheckException( "Bad invocation object reference");
        }
        if ( return_stack!=null)
        	new_stack.push( return_stack);
        return new_stack;
    }
}
