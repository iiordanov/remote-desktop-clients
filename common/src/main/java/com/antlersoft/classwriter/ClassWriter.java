
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
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;

public class ClassWriter
{
    private int magic;
    private int majorVersion;
    private int minorVersion;
    int accessFlags;
    int thisClassIndex;
    int superClassIndex;
	ArrayList<CPInfo> constantPool;
	ArrayList<Integer> interfaces;
    ArrayList<FieldInfo> fields;
    ArrayList<MethodInfo> methods;
	AttributeList attributes;

    public static final short CONSTANT_Utf8=1;
    public static final short CONSTANT_Integer=3;
    public static final short CONSTANT_Float=4;
    public static final short CONSTANT_Long=5;
    public static final short CONSTANT_Double=6;
    public static final short CONSTANT_Class=7;
    public static final short CONSTANT_String=8;
    public static final short CONSTANT_Fieldref=9;
    public static final short CONSTANT_Methodref=10;
    public static final short CONSTANT_InterfaceMethodref=11;
    public static final short CONSTANT_NameAndType=12;

    public static final int ACC_PUBLIC=0x0001;
    public static final int ACC_PRIVATE=0x0002;
    public static final int ACC_PROTECTED=0x0004;
    public static final int ACC_STATIC=0x0008;
    public static final int ACC_FINAL=0x0010;
    public static final int ACC_SUPER=0x0020;
    public static final int ACC_VOLATILE=0x0040;
    public static final int ACC_TRANSIENT=0x0080;
    public static final int ACC_INTERFACE=0x0200;
    public static final int ACC_ABSTRACT=0x0400;

    public ClassWriter()
    {
        clearClass();
    }

    public void emptyClass( int flags, String className, String superClass)
    {
        clearClass();
        constantPool.add( null);
        magic=0xCAFEBABE;
        majorVersion=45;
        minorVersion=32767;
        accessFlags=flags|ACC_SUPER;
        thisClassIndex=getClassIndex( className);
        superClassIndex=getClassIndex( superClass);
     }

    public void readClass( InputStream is)
		throws IOException, CodeCheckException
    {
        clearClass();

		DataInputStream classStream=new DataInputStream( is);

		magic=classStream.readInt();
		minorVersion=classStream.readUnsignedShort();
		majorVersion=classStream.readUnsignedShort();
  		constantPool.add( null);
		int constantPoolCount=classStream.readUnsignedShort();
  		int i;
		for ( i=1; i<constantPoolCount; i++)
		{
  			CPInfo poolEntry=readConstant( classStream);
		    constantPool.add( poolEntry);
		    if ( poolEntry.tag==CONSTANT_Double ||
				poolEntry.tag==CONSTANT_Long)
    		{
				i++;
    			constantPool.add( null);
    		}
		}
		accessFlags=classStream.readUnsignedShort();
		thisClassIndex=classStream.readUnsignedShort();
		superClassIndex=classStream.readUnsignedShort();
		int interfacesCount=classStream.readUnsignedShort();
		for ( i=0; i<interfacesCount; i++)
		{
		    interfaces.add( new Integer( classStream.readUnsignedShort()));
		}
		int fieldsCount=classStream.readUnsignedShort();
		for ( i=0; i<fieldsCount; i++)
		{
		    fields.add( readFieldInfo( classStream));
		}
		int methodsCount=classStream.readUnsignedShort();
		for ( i=0; i<methodsCount; i++)
		{
		    methods.add( readMethodInfo( classStream));
		}
  		attributes.read( classStream);
    }
    
    public AttributeList getAttributeList() { return attributes; }

    public void writeClass( OutputStream is)
		throws IOException
    {
		DataOutputStream classStream=new DataOutputStream( is);

		classStream.writeInt( magic);
    	classStream.writeShort( minorVersion);
  		classStream.writeShort( majorVersion);
     	classStream.writeShort( constantPool.size());
  		Iterator<CPInfo> i=constantPool.iterator();
    	i.next();	// Skip initial, not really there entry
		for ( ; i.hasNext();)
		{
  			CPInfo poolEntry=i.next();
  			poolEntry.write( classStream);
     		// Constant pool array must have null entry after long or double
		    if ( poolEntry.tag==CONSTANT_Double ||
				poolEntry.tag==CONSTANT_Long)
				i.next();
		}
		classStream.writeShort( accessFlags);
		classStream.writeShort( thisClassIndex);
		classStream.writeShort( superClassIndex);
  		classStream.writeShort( interfaces.size());
		for ( Iterator<Integer> j=interfaces.iterator(); j.hasNext();)
		{
		    classStream.writeShort( j.next());
		}
  		classStream.writeShort( fields.size());
		for ( Iterator<FieldInfo> j=fields.iterator(); j.hasNext();)
		{
		    j.next().write( classStream);
		}
  		classStream.writeShort( methods.size());
		for ( Iterator<MethodInfo> j=methods.iterator(); j.hasNext();)
		{
		    j.next().write( classStream);
		}
  		attributes.write( classStream);
    }

    public int getFlags()
    {
        return accessFlags;
    }

    public Collection<MethodInfo> getMethods()
    {
        return Collections.unmodifiableCollection( methods);
    }

    public Collection<FieldInfo> getFields()
    {
        return Collections.unmodifiableCollection( fields);
    }

    public void removeFromFields( Collection<FieldInfo> toRemove)
    {
        fields.removeAll( toRemove);
    }

    public void setBase( String newBaseClass)
    {
        superClassIndex=getClassIndex( newBaseClass);
    }

    public Collection<Integer> getInterfaces()
    {
        return Collections.unmodifiableCollection( interfaces);
    }

    public MethodInfo addMethod( int flags, String name, String descriptor)
    {
        MethodInfo result=new MethodInfo( flags, name, descriptor, this);
        methods.add( result);
        return result;
    }

    public CPTypeRef getTypeRef( int index)
    {
        return (CPTypeRef)constantPool.get( index);
    }

    public FieldInfo addField( int flags, String name, String descriptor)
    {
        FieldInfo result=new FieldInfo( flags, name, descriptor, this);
        fields.add( result);
        return result;
    }

    public int addInterface( String interfaceName)
    {
        int result=getClassIndex( interfaceName);
        interfaces.add( new Integer( result));
        return result;
    }

    public int getCurrentClassIndex()
    {
        return thisClassIndex;
    }

    public int getSuperClassIndex()
    {
        return superClassIndex;
    }

    public boolean isDeprecated()
    {
    	return attributes.getAttributeByType( DeprecatedAttribute.typeString)!=null;
    }

    private void clearClass()
    {
    	constantPool=new ArrayList<CPInfo>();
     	interfaces=new ArrayList<Integer>();
      	fields=new ArrayList<FieldInfo>();
        methods=new ArrayList<MethodInfo>();
        attributes=new AttributeList( this);
    }

    CPInfo readConstant( DataInputStream classStream)
		throws IOException, CodeCheckException
    {
		short tag=(short)classStream.readUnsignedByte();
		CPInfo result;
		switch ( tag)
		{
		    case CONSTANT_Utf8 :
				result=new CPUtf8( classStream);
				break;
		    case CONSTANT_Integer :
				result=new CPInteger( classStream);
				break;
		    case CONSTANT_Float :
				result=new CPFloat( classStream);
				break;
		    case CONSTANT_Long :
				result=new CPLong( classStream);
				break;
		    case CONSTANT_Double :
				result=new CPDouble( classStream);
				break;
		    case CONSTANT_Class :
				result=new CPClass( classStream);
				break;
		    case CONSTANT_String :
				result=new CPString( classStream);
				break;
		    case CONSTANT_Fieldref :
		    case CONSTANT_Methodref :
		    case CONSTANT_InterfaceMethodref :
				result=new CPTypeRef( tag, classStream);
				break;
		    case CONSTANT_NameAndType :
				result=new CPNameAndType( classStream);
				break;
		    default :
				throw new CodeCheckException( "Unknown constant type "+(int)tag);
		}
		return result;
    }

	static class CPInfo
	{
		short tag;
		CPInfo( short t)
		{
			tag=t;
		}
		void write( DataOutputStream classStream)
			throws IOException
		{
			classStream.writeByte( tag);
		}
	}

    static class CPUtf8 extends CPInfo
    {
		String value;
		CPUtf8( DataInputStream classStream)
		    throws IOException
		{
		    super( ClassWriter.CONSTANT_Utf8);
      		value=classStream.readUTF();
		}

        CPUtf8( String val)
        {
		    super( ClassWriter.CONSTANT_Utf8);
            value=val;
        }

  		void write( DataOutputStream classStream)
    		throws IOException
    	{
     		super.write( classStream);
       		classStream.writeUTF( value);
     	}
    }

    public final String getString( int index)
    {
		return ((CPUtf8)constantPool.get(index)).value;
    }

    public final int findStringIndex( String toFind)
    {
    	int index=0;
     	for ( Iterator<CPInfo> i=constantPool.iterator(); i.hasNext(); index++)
        {
        	Object constant=i.next();
         	if ( constant!=null && constant instanceof CPUtf8)
          	{
           		if ( ((CPUtf8)constant).value.equals( toFind))
             		return index;
           	}
        }

        return -1;
    }

    public final int getStringIndex( String toFind)
    {
        int index=findStringIndex( toFind);
        if ( index== -1)
        {
            constantPool.add( new CPUtf8( toFind));
            index=constantPool.size()-1;
        }
        return index;
    }

    public final int getClassIndex( String toFind)
    {
        int stringIndex=findStringIndex( toFind);
        if ( stringIndex== -1)
        {
            constantPool.add( new CPUtf8( toFind));
            stringIndex=constantPool.size()-1;
        }
        else
        {
            int index=0;
         	for ( Iterator<CPInfo> i=constantPool.iterator(); i.hasNext(); index++)
            {
            	Object constant=i.next();
             	if ( constant!=null && constant instanceof CPClass)
              	{
               		if ( ((CPClass)constant).nameIndex==stringIndex)
                 		return index;
               	}
            }
        }
        CPClass classRef=new CPClass( stringIndex);
        constantPool.add( classRef);
        return constantPool.size()-1;
    }

    public final int getStringConstantIndex( String toFind)
    {
        int stringIndex=findStringIndex( toFind);
        if ( stringIndex== -1)
        {
            constantPool.add( new CPUtf8( toFind));
            stringIndex=constantPool.size()-1;
        }
        else
        {
            int index=0;
         	for ( Iterator<CPInfo> i=constantPool.iterator(); i.hasNext(); index++)
            {
            	Object constant=i.next();
             	if ( constant!=null && constant instanceof CPString)
              	{
               		if ( ((CPString)constant).valueIndex==stringIndex)
                 		return index;
               	}
            }
        }
        CPString stringRef=new CPString( stringIndex);
        constantPool.add( stringRef);
        return constantPool.size()-1;
    }

    public String getStringConstant( int index)
    {
        Object constant=constantPool.get( index);
        if ( constant instanceof CPString)
            return getString( ((CPString)constant).valueIndex);
        if ( constant instanceof CPInteger)
            return Integer.toString( ((CPInteger)constant).value);
        return "???";
    }

    public String getIfStringConstant( int index)
    {
        String result=null;
        Object constant=constantPool.get( index);
        if ( constant instanceof CPString)
            result= getString( ((CPString)constant).valueIndex);

    	return result;
    }

    final int getNameAndTypeIndex( String name, String descriptor)
    {
        int nameIndex=getStringIndex( name);
        int descriptorIndex=getStringIndex( descriptor);
        int index=0;
        for ( Iterator<CPInfo> i=constantPool.iterator(); i.hasNext(); index++)
        {
            Object constant=i.next();
            if ( constant instanceof CPNameAndType)
            {
                CPNameAndType nt=(CPNameAndType)constant;
                if ( nt.nameIndex==nameIndex && nt.descriptorIndex==descriptorIndex)
                    return index;
            }
        }
        constantPool.add( new CPNameAndType( nameIndex, descriptorIndex));
        return constantPool.size()-1;
    }

    public final int getReferenceIndex( int tag, String className, String name, String descriptor)
    {
        int classIndex=getClassIndex( className);
        int nameAndTypeIndex=getNameAndTypeIndex( name, descriptor);
        int index=0;
        for ( Iterator<CPInfo> i=constantPool.iterator(); i.hasNext(); index++)
        {
            Object constant=i.next();
            if ( constant instanceof CPTypeRef)
            {
                CPTypeRef tr=(CPTypeRef)constant;
                if ( tr.classIndex==classIndex && tr.nameAndTypeIndex==nameAndTypeIndex)
                    return index;
            }
        }
        constantPool.add( new CPTypeRef( (short)tag, classIndex, nameAndTypeIndex));
        return constantPool.size()-1;
    }

    public String getInternalClassName( int classIndex)
    {
        return getString( ((CPClass)constantPool.get( classIndex)).nameIndex);
    }

    public String getSourceFile()
    {
        SourceFileAttribute sfa=(SourceFileAttribute)attributes.getAttributeByType( SourceFileAttribute.typeString);
        if ( sfa!=null)
        {
            return sfa.getSourceFile();
        }
        String internalName=getInternalClassName( getCurrentClassIndex());
        int lastSlash=internalName.lastIndexOf( '/');
        if ( lastSlash!= -1)
            internalName=internalName.substring( lastSlash+1);
        int firstPeriod=internalName.indexOf( '$');
        if ( firstPeriod!= -1)
            internalName=internalName.substring( 0, firstPeriod);
        return internalName+".java";
    }

    public String getClassName( int classIndex)
    {
		return TypeParse.convertFromInternalClassName(
            getInternalClassName( classIndex));
    }

    public class CPTypeRef extends CPInfo
    {
		int classIndex;
		int nameAndTypeIndex;

		CPTypeRef( short t, DataInputStream classStream)
		    throws IOException
		{
		    super( t);
		    classIndex=classStream.readUnsignedShort();
		    nameAndTypeIndex=classStream.readUnsignedShort();
		}

        CPTypeRef( short t, int ci, int nti)
        {
            super( t);
            classIndex=ci;
            nameAndTypeIndex=nti;
        }

        public int getClassIndex()
        {
            return classIndex;
        }

		public String getSymbolName()
  		{
        	return getString( ((CPNameAndType)constantPool.get(
         		nameAndTypeIndex)).nameIndex);
        }

        public String getSymbolType()
        {
            return getString( ((CPNameAndType)constantPool.get(
            	nameAndTypeIndex)).descriptorIndex);
        }

        public void setSymbolType( String newType)
        {
            ((CPNameAndType)constantPool.get( nameAndTypeIndex)).descriptorIndex
                =getStringIndex( newType);
        }

  		void write( DataOutputStream classStream)
    		throws IOException
    	{
     		super.write( classStream);
       		classStream.writeShort( classIndex);
         	classStream.writeShort( nameAndTypeIndex);
        }
    }

    static class CPNameAndType extends CPInfo
    {
		int nameIndex;
		int descriptorIndex;

		CPNameAndType( DataInputStream classStream)
		    throws IOException
		{
		    super( ClassWriter.CONSTANT_NameAndType);
		    nameIndex=classStream.readUnsignedShort();
		    descriptorIndex=classStream.readUnsignedShort();
		}

        CPNameAndType( int ni, int di)
        {
            super( ClassWriter.CONSTANT_NameAndType);
            nameIndex=ni;
            descriptorIndex=di;
        }

  		void write( DataOutputStream classStream)
    		throws IOException
      	{
       		super.write( classStream);
         	classStream.writeShort( nameIndex);
          	classStream.writeShort( descriptorIndex);
        }
    }

    static class CPClass extends CPInfo
    {
		int nameIndex;
		CPClass( DataInputStream classStream)
		    throws IOException
		{
		    super( ClassWriter.CONSTANT_Class);
		    nameIndex=classStream.readUnsignedShort();
		}

        CPClass( int index)
        {
            super( ClassWriter.CONSTANT_Class);
            nameIndex=index;
        }

  		void write( DataOutputStream classStream)
    		throws IOException
      	{
       		super.write( classStream);
         	classStream.writeShort( nameIndex);
        }
    }

    static class CPString extends CPInfo
    {
		int valueIndex;
		CPString( DataInputStream classStream)
		    throws IOException
		{
		    super( ClassWriter.CONSTANT_String);
		    valueIndex=classStream.readUnsignedShort();
		}

        CPString( int index)
        {
            super( ClassWriter.CONSTANT_String);
            valueIndex=index;
        }

  		void write( DataOutputStream classStream)
    		throws IOException
      	{
       		super.write( classStream);
         	classStream.writeShort( valueIndex);
        }
    }

    static class CPInteger extends CPInfo
    {
		int value;
		CPInteger( DataInputStream classStream)
		    throws IOException
		{
		    super( ClassWriter.CONSTANT_Integer);
		    value=classStream.readInt();
		}

  		void write( DataOutputStream classStream)
    		throws IOException
      	{
       		super.write( classStream);
         	classStream.writeInt( value);
        }
    }

    static class CPFloat extends CPInfo
    {
		float value;
		CPFloat( DataInputStream classStream)
		    throws IOException
		{
		    super( ClassWriter.CONSTANT_Float);
		    value=classStream.readFloat();
		}

  		void write( DataOutputStream classStream)
    		throws IOException
      	{
       		super.write( classStream);
         	classStream.writeFloat( value);
        }
    }

    static class CPLong extends CPInfo
    {
		long value;
		CPLong( DataInputStream classStream)
		    throws IOException
		{
		    super( ClassWriter.CONSTANT_Long);
		    value=classStream.readLong();
		}

  		void write( DataOutputStream classStream)
    		throws IOException
      	{
       		super.write( classStream);
         	classStream.writeLong( value);
        }
    }

    static class CPDouble extends CPInfo
    {
		double value;
		CPDouble( DataInputStream classStream)
		    throws IOException
		{
		    super( ClassWriter.CONSTANT_Double);
		    value=classStream.readDouble();
		}

  		void write( DataOutputStream classStream)
    		throws IOException
      	{
       		super.write( classStream);
         	classStream.writeDouble( value);
        }
    }

    private FieldInfo readFieldInfo( DataInputStream classStream)
    	throws IOException
    {
    	return new FieldInfo( classStream, this);
    }

    private MethodInfo readMethodInfo( DataInputStream classStream)
    	throws IOException
    {
    	return new MethodInfo( classStream, this);
    }
}
