
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

import java.util.ArrayList;
import java.util.Iterator;
import java.util.Stack;

public class CheckedInstruction
{
    public Instruction instruction;
    public Stack stack;
    public ArrayList previousCheckedInstructions;

    CheckedInstruction( Instruction i, Stack s)
    {
        instruction=i;
        stack=s;
        previousCheckedInstructions=new ArrayList();
    }

    int getStackDepth()
    {
        int depth=0;
        for ( Iterator i=stack.iterator(); i.hasNext();)
        {
            Object o=i.next();
            if ( o==ProcessStack.CAT1)
                depth++;
            if ( o==ProcessStack.CAT2)
                depth+=2;
        }
        return depth;
    }
}