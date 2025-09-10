
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

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

import java.util.Collection;
import java.util.ArrayList;
import java.util.Iterator;

public class AttributeList implements Cloneable
{
	private ArrayList attributes;
 	private ClassWriter containing;

 	public AttributeList( ClassWriter contains)
  	{
   		attributes=null;
     	containing=contains;
   	}

    public Collection getAttributes()
    {
    	return (Collection)attributes.clone();
    }

    public Attribute getAttributeByType( String type)
    {
        if ( attributes==null)
            return null;
    	for ( Iterator i=attributes.iterator(); i.hasNext();)
        {
        	Attribute next=(Attribute)i.next();
         	if ( next.getTypeString().equals( type))
            {
            	return next;
            }
        }
        return null;
    }

    public ClassWriter getCurrentClass()
    {
        return containing;
    }

    public void addAttribute( Attribute toAdd)
    {
        if ( attributes==null)
        	attributes=new ArrayList();
        containing.getStringIndex( toAdd.getTypeString());
        attributes.add( toAdd);
    }

    public void read( DataInputStream classStream)
    	throws IOException
    {
   	    int attributesCount=classStream.readUnsignedShort();
	    attributes=new ArrayList( attributesCount);
	    for ( int i=0; i<attributesCount; i++)
	    {
			attributes.add( readAttribute( classStream));
	    }
	}

	private Attribute readAttribute( DataInputStream classStream)
	    throws IOException
	{
	    int nameIndex=classStream.readUnsignedShort();
	    int length=classStream.readInt();
	    String type=containing.getString( nameIndex);
     	Attribute value;
	    if ( type.equals( SourceFileAttribute.typeString))
	    {
			value=new SourceFileAttribute( classStream, containing);
	    }
	    else /* if ( type.equals( "ConstantValue"))
	    {
			value=constantPool[classStream.readUnsignedShort()];
	    }
	    else */ if ( type.equals( CodeAttribute.typeString))
	    {
			value=new CodeAttribute( classStream, containing);
	    }
	    else if ( type.equals( ExceptionsAttribute.typeString))
	    {
			value=new ExceptionsAttribute( classStream);
	    }
	    else if ( type.equals( LineNumberTableAttribute.typeString))
	    {
			value=new LineNumberTableAttribute( classStream);
	    }
	    else if ( type.equals( LocalVariableTableAttribute.typeString))
	    {
			value=new LocalVariableTableAttribute( classStream);
	    }
	    else if ( type.equals( RuntimeVisibleAnnotationsAttribute.typeString))
	    {
	    	value=new RuntimeVisibleAnnotationsAttribute(classStream);
	    }
	    else if ( type.equals( RuntimeInvisibleAnnotationsAttribute.typeString))
	    {
	    	value=new RuntimeInvisibleAnnotationsAttribute(classStream);
	    }
	    else if ( type.equals( RuntimeVisibleParameterAnnotationsAttribute.typeString))
	    {
	    	value=new RuntimeVisibleParameterAnnotationsAttribute(classStream);
	    }
	    else if ( type.equals( RuntimeInvisibleParameterAnnotationsAttribute.typeString))
	    {
	    	value=new RuntimeInvisibleParameterAnnotationsAttribute(classStream);
	    }
	    else
	    {
			/* Unknown type -- pass through silently */
   			value=new UnknownAttribute( length, type, classStream);
	    }

     	return value;
	}

 	public void write( DataOutputStream classStream)
  		throws IOException
    {
        if ( attributes==null)
        {
            classStream.writeShort(0);
            return;
        }
    	classStream.writeShort( attributes.size());
     	for ( Iterator i=attributes.iterator(); i.hasNext();)
      	{
       		Attribute attribute=(Attribute)i.next();
	 		classStream.writeShort( containing.findStringIndex(
    			attribute.getTypeString()));
			ByteArrayOutputStream byteStream=new ByteArrayOutputStream();

	  		attribute.write( new DataOutputStream( byteStream));
	    	byte[] valueBytes=byteStream.toByteArray();
	     	classStream.writeInt( valueBytes.length);
	    	classStream.write( valueBytes);
        }
    }
}