/**
 * Copyright (c) 2008 Michael A. MacDonald
 */
package com.antlersoft.classwriter;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;

import com.antlersoft.classwriter.ClassWriter.CPClass;
import com.antlersoft.classwriter.ClassWriter.CPDouble;
import com.antlersoft.classwriter.ClassWriter.CPFloat;
import com.antlersoft.classwriter.ClassWriter.CPInteger;
import com.antlersoft.classwriter.ClassWriter.CPLong;
import com.antlersoft.classwriter.ClassWriter.CPNameAndType;
import com.antlersoft.classwriter.ClassWriter.CPString;
import com.antlersoft.classwriter.ClassWriter.CPTypeRef;
import com.antlersoft.classwriter.ClassWriter.CPUtf8;

/**
 * An annotation stored in the Java class file as described in JSR-175
 * @author Michael A. MacDonald
 *
 */
public class Annotation implements AnnotationInfo {
	private int type;
	private int numElementValues;
	private ElementValuePair[] elementValuePairs;
	
	Annotation( DataInputStream classStream)
	throws IOException
	{
		type=classStream.readUnsignedShort();
		numElementValues=classStream.readUnsignedShort();
		elementValuePairs=new ElementValuePair[numElementValues];
		for ( int i=0; i<numElementValues; ++i)
		{
			elementValuePairs[i]=new ElementValuePair(classStream);
		}
	}
	
	void write(DataOutputStream classStream) throws IOException
	{
		classStream.writeShort(type);
		classStream.writeShort(numElementValues);
		for ( int i=0; i<numElementValues; ++i)
		{
			elementValuePairs[i].write(classStream);
		}
	}
	
	public ElementValuePair[] getNameValuePairs()
	{
		return elementValuePairs.clone();
	}
	
	/**
	 * @param elementName Name of value to find in the annotation
	 * @return Element value converted to string if it exists and can be so converted; otherwise the empty
	 * string
	 */
	public String getElementValueAsString( ClassWriter cw, String elementName)
	{
		for ( ElementValuePair p : elementValuePairs)
		{
			if ( elementName.equals(cw.getString(p.name)))
			{
				StringBuilder sb=new StringBuilder();
				ArrayList<String> strings=new ArrayList<String>();
				ArrayList<Annotation> annotations=new ArrayList<Annotation>();
				p.value.value.gatherAnnotationInfo(cw, annotations, strings);
				for ( String s : strings)
					sb.append(s);
				return sb.toString();
			}
		}
		
		return "";
	}
	
	/**
	 * Return an object representing the value of an annotation element, or null if the named
	 * element is not defined within the annotation.  Note that this does not access the
	 * default value defined for an annotation type, since that is available only at runtime.
	 * <p>
	 * For string-valued annotations, returns a string.
	 * <p>
	 * For other constant types, returns the Java object for that type (Boolean, Integer...)
	 * <p>
	 * For annotation-valued annotations, returns an Annotation object.
	 * <p>
	 * For array-valued annotations, returns an ElementValue[] object.
	 * <p>
	 * For enum-valued objects, returns a string representing the name of the enum value.
	 * @param cw ClassWriter with information about the containing class
	 * @param elementName Annotation element name sought
	 * @return Object with value of annotation element if it is defined in the class, or null if
	 * it is not defined.
	 */
	public Object getElementValue( ClassWriter cw, String elementName)
	{
		Object result=null;
		
		for ( ElementValuePair p : elementValuePairs)
		{
			if ( elementName.equals(cw.getString(p.name)))
			{
				result=p.value.value.getObject(cw);
			}
		}
		
		return result;
	}
	
	/**
	 * 
	 * @return id in constant pool of string constant that corresponds to the internal name
	 * of the Java type of the interface defining the annotation.
	 */
	public int getClassIndex()
	{
		return type;
	}
	
	/* (non-Javadoc)
	 * @see com.antlersoft.classwriter.AnnotationInfo#gatherAnnotationInfo(com.antlersoft.classwriter.ClassWriter, java.util.Collection, java.util.Collection)
	 */
	public void gatherAnnotationInfo(ClassWriter container, Collection<Annotation> annotations, Collection<String> strings) {
		for ( ElementValuePair p : elementValuePairs)
			p.gatherAnnotationInfo( container, annotations, strings);
	}

	/**
	 * One element name-value pair in an annotation.
	 * @author Michael A. MacDonald
	 *
	 */
	public static class ElementValuePair implements AnnotationInfo
	{
		private int name;
		private ElementValue value;
		
		ElementValuePair( DataInputStream classStream)
		throws IOException
		{
			name=classStream.readUnsignedShort();
			value=new ElementValue( classStream);
		}
		
		void write( DataOutputStream classStream)
		throws IOException
		{
			classStream.writeShort( name);
			value.write(classStream);
		}
		
		int getNameIndex()
		{
			return name;
		}

		/* (non-Javadoc)
		 * @see com.antlersoft.classwriter.AnnotationInfo#gatherAnnotationInfo(com.antlersoft.classwriter.ClassWriter, java.util.Collection, java.util.Collection)
		 */
		public void gatherAnnotationInfo(ClassWriter container, Collection<Annotation> annotations, Collection<String> strings) {
			strings.add( container.getStringConstant(name));
			value.value.gatherAnnotationInfo(container, annotations, strings);
		}
	}
	
	/**
	 * Discriminated union of value types
	 * @author Michael A. MacDonald
	 *
	 */
	public static class ElementValue
	{
		private int type;
		private ValueBase value;
		
		ElementValue( DataInputStream classStream)
		throws IOException
		{
			type=classStream.readUnsignedByte();
			value=ValueBase.createValueBase( type, classStream);
		}
		
		void write( DataOutputStream classStream)
		throws IOException
		{
			classStream.write(type);
			value.write( classStream);
		}
	}
	
	/**
	 * Base class for value types within an ElementValue.
	 * @author Michael A. MacDonald
	 *
	 */
	public abstract static class ValueBase implements AnnotationInfo
	{
		static ValueBase createValueBase( int type, DataInputStream classStream)
		throws IOException
		{
			ValueBase result;
			switch ( type )
			{
			case '@' : result=new AnnotationValue(classStream); break;
			case 'c' : result=new ClassValue(classStream); break;
			case 'e' : result=new EnumValue(classStream); break;
			case '[' : result=new ArrayValue(classStream); break;
			default :
				result=new ConstPoolValue( classStream);
			}
			
			return result;
		}
		
		/**
		 * Write the value to the datastream as defined for the type
		 * @param classStream Class file output stream
		 * @throws IOException
		 */
		abstract void write( DataOutputStream classStream) throws IOException;
		
		abstract Object getObject(ClassWriter cw);

		/*
		 * @see com.antlersoft.classwriter.AnnotationInfo#gatherAnnotationInfo(com.antlersoft.classwriter.ClassWriter, java.util.Collection, java.util.Collection)
		 */
		public void gatherAnnotationInfo(ClassWriter container, Collection<Annotation> annotations, Collection<String> strings) {
			// Default implementation does nothing
		}
	}
	
	/**
	 * Annotation value is a constant in the constant pool
	 * @author Michael A. MacDonald
	 *
	 */
	public static class ConstPoolValue extends ValueBase
	{
		private int poolIndex;
		
		ConstPoolValue( DataInputStream classStream)
		throws IOException
		{
			poolIndex=classStream.readUnsignedShort();
		}
		
		@Override
		void write( DataOutputStream classStream) throws IOException
		{
			classStream.writeShort(poolIndex);
		}
		
		@Override
		Object getObject(ClassWriter cw)
		{
			Object result=null;
			ClassWriter.CPInfo entry=cw.constantPool.get(poolIndex);
			
			switch ( entry.tag)
			{
			case ClassWriter.CONSTANT_Double :
				result=new Double(((ClassWriter.CPDouble)entry).value);
				break;
			case ClassWriter.CONSTANT_Float :
				result=new Float(((ClassWriter.CPFloat)entry).value);
				break;
			case ClassWriter.CONSTANT_Integer :
				result=new Integer(((ClassWriter.CPInteger)entry).value);
				break;
			case ClassWriter.CONSTANT_Long :
				result=new Long(((ClassWriter.CPLong)entry).value);
				break;
			case ClassWriter.CONSTANT_String :
				result=cw.getStringConstant(((ClassWriter.CPString)entry).valueIndex);
				break;
			case ClassWriter.CONSTANT_Utf8 :
				result=((ClassWriter.CPUtf8)entry).value;
				break;
			default :
			}
			return result;
		}
		/* (non-Javadoc)
		 * @see com.antlersoft.classwriter.AnnotationInfo#gatherAnnotationInfo(com.antlersoft.classwriter.ClassWriter, java.util.Collection, java.util.Collection)
		 */
		public void gatherAnnotationInfo(ClassWriter container, Collection<Annotation> annotations, Collection<String> strings) {
			ClassWriter.CPInfo info=container.constantPool.get(poolIndex);
			String s=null;
			if (info instanceof ClassWriter.CPUtf8)
				s=container.getString(poolIndex);
			if ( s!=null)
				strings.add(s);
		}
	}
	
	/**
	 * Annotation value is an enumeration value
	 * @author Michael A. MacDonald
	 *
	 */
	public static class EnumValue extends ValueBase
	{
		private int enumType;
		private int enumName;
		
		EnumValue( DataInputStream classStream)
		throws IOException
		{
			enumType=classStream.readUnsignedShort();
			enumName=classStream.readUnsignedShort();
		}
		
		/* (non-Javadoc)
		 * @see com.antlersoft.classwriter.Annotation.ValueBase#write(java.io.DataOutputStream)
		 */
		@Override
		void write(DataOutputStream classStream) throws IOException {
			classStream.writeShort(enumType);
			classStream.writeShort(enumName);
		}

		@Override
		Object getObject(ClassWriter cw) {
			return cw.getString(enumName);
		}
	}
	/**
	 * Annotation value is string in pool representing a type
	 * @author Michael A. MacDonald
	 *
	 */
	public static class ClassValue extends ValueBase
	{
		private int poolIndex;
		
		ClassValue( DataInputStream classStream)
		throws IOException
		{
			poolIndex=classStream.readUnsignedShort();
		}
		
		@Override
		void write( DataOutputStream classStream) throws IOException
		{
			classStream.writeShort(poolIndex);
		}

		@Override
		Object getObject(ClassWriter cw) {
			return cw.getString(poolIndex);
		}
	}
	/**
	 * Annotation value is an annotation
	 * @author Michael A. MacDonald
	 *
	 */
	public static class AnnotationValue extends ValueBase
	{
		private Annotation annotation;
		
		AnnotationValue( DataInputStream classStream)
		throws IOException
		{
			annotation=new Annotation(classStream);
		}
		
		@Override
		void write( DataOutputStream classStream) throws IOException
		{
			annotation.write(classStream);
		}

		/* (non-Javadoc)
		 * @see com.antlersoft.classwriter.Annotation.ValueBase#gatherAnnotationInfo(com.antlersoft.classwriter.ClassWriter, java.util.Collection, java.util.Collection)
		 */
		@Override
		public void gatherAnnotationInfo(ClassWriter container, Collection<Annotation> annotations, Collection<String> strings) {
			annotations.add( annotation);
			annotation.gatherAnnotationInfo(container, annotations, strings);
		}

		@Override
		Object getObject(ClassWriter cw) {
			return annotation;
		}
	}
	/**
	 * Annotation value is an array of values
	 * @author Michael A. MacDonald
	 *
	 */
	public static class ArrayValue extends ValueBase {
		private int numElements;
		private ElementValue[] elementValues;
		
		ArrayValue( DataInputStream classStream)
		throws IOException
		{
			numElements=classStream.readUnsignedShort();
			elementValues=new ElementValue[numElements];
			for ( int i=0; i<numElements; ++i)
			{
				elementValues[i]=new ElementValue(classStream);
			}
		}
		
		/* (non-Javadoc)
		 * @see com.antlersoft.classwriter.Annotation.ValueBase#write(java.io.DataOutputStream)
		 */
		@Override
		void write(DataOutputStream classStream) throws IOException {
			classStream.writeShort(numElements);
			for ( int i=0; i<numElements; ++i)
			{
				elementValues[i].write(classStream);
			}
		}

		/* (non-Javadoc)
		 * @see com.antlersoft.classwriter.Annotation.ValueBase#gatherAnnotationInfo(com.antlersoft.classwriter.ClassWriter, java.util.Collection, java.util.Collection)
		 */
		@Override
		public void gatherAnnotationInfo(ClassWriter container, Collection<Annotation> annotations, Collection<String> strings) {
			for ( ElementValue ev : elementValues)
			{
				ev.value.gatherAnnotationInfo(container, annotations, strings);
			}
		}

		@Override
		Object getObject(ClassWriter cw) {
			return elementValues;
		}
	}
}
