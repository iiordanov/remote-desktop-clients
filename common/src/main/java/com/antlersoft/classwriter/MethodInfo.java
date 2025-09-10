
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

import java.io.DataInputStream;
import java.io.IOException;

public class MethodInfo extends FieldInfo
{
	MethodInfo( DataInputStream dis, ClassWriter containing)
 		throws IOException
    {
    	super( dis, containing);
    }

    MethodInfo( int flags, String name, String descriptor,
        ClassWriter contains)
    {
        super( flags, name, descriptor, contains);
        attributes.addAttribute( new CodeAttribute( contains));
     }

	public CodeAttribute getCodeAttribute()
 	{
  		return (CodeAttribute)attributes.getAttributeByType(
    		CodeAttribute.typeString);
  	}
}