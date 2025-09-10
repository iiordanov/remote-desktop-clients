
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
import java.io.DataOutputStream;
import java.io.IOException;

public class FieldInfo
{
	int accessFlags;
	int nameIndex;
	int descriptorIndex;
 	AttributeList attributes;

	FieldInfo( DataInputStream classStream, ClassWriter contains)
	    throws IOException
	{
	    accessFlags=classStream.readUnsignedShort();
	    nameIndex=classStream.readUnsignedShort();
	    descriptorIndex=classStream.readUnsignedShort();
     	attributes=new AttributeList( contains);
      	attributes.read( classStream);
	}

    FieldInfo( int flags, String name, String descriptor, ClassWriter contains)
    {
        nameIndex=contains.getStringIndex( name);
        descriptorIndex=contains.getStringIndex( descriptor);
        accessFlags=flags;
        attributes=new AttributeList( contains);
    }
    
    public AttributeList getAttributeList()
    {
    	return attributes;
    }

    public String getName()
    {
        return attributes.getCurrentClass().getString( nameIndex);
    }

    public String getType()
    {
        return attributes.getCurrentClass().getString( descriptorIndex);
    }

    public int getFlags()
    {
        return accessFlags;
    }

    public void setType( String newType)
    {
        descriptorIndex=attributes.getCurrentClass().getStringIndex( newType);
    }
    
    public boolean isDeprecated()
    {
    	return attributes.getAttributeByType( DeprecatedAttribute.typeString)!=null;
    }

	void write( DataOutputStream classStream)
		throws IOException
	{
   		classStream.writeShort( accessFlags);
     	classStream.writeShort( nameIndex);
      	classStream.writeShort( descriptorIndex);
        attributes.write( classStream);
   	}
}