/**
 * Copyright (C) 2008 Michael A. MacDonald
 */
package com.antlersoft.android.dbgen;

import java.io.IOException;
import java.util.ArrayList;

import com.antlersoft.classwriter.*;

import com.antlersoft.android.db.FieldType;
import com.antlersoft.android.db.FieldVisibility;

/**
 * @author Michael A. MacDonald
 *
 */
public class SourceFileGenerator {
	
	private SourceInterface sourceBase;
	
	static String[][] CODE_TO_JAVA_TYPE = {
		{ TypeParse.ARG_BOOLEAN, "boolean" },
		{ TypeParse.ARG_BYTE, "byte" },
		{ TypeParse.ARG_CHAR, "char" },
		{ TypeParse.ARG_DOUBLE, "double" },
		{ TypeParse.ARG_FLOAT, "float" },
		{ TypeParse.ARG_INT, "int" },
		{ TypeParse.ARG_LONG, "long" },
		{ TypeParse.ARG_SHORT, "short" }
	};
	
	public SourceFileGenerator( SourceInterface sourceBase)
	{
		this.sourceBase=sourceBase;
	}
	
	/**
	 * If a read class has annotations indicating it is meant to define a table, generate the java code files
	 * for that table.
	 * @param cw ClassWriter that read .class file
	 * @return true if any files were generated
	 * @throws IOException On IO error
	 */
	public boolean generate( ClassWriter cw) throws IOException, SourceInterface.SIException
	{
		boolean result=false;
		TableDefinition td=new TableDefinition();
		if ( (cw.getFlags() & ClassWriter.ACC_INTERFACE)!=0)
		{
			RuntimeVisibleAnnotationsAttribute annotations=(RuntimeVisibleAnnotationsAttribute)cw.getAttributeList().getAttributeByType( RuntimeInvisibleAnnotationsAttribute.typeString);
			if ( annotations!=null)
			{
				for ( Annotation a : annotations.getAnnotations())
				{
					if ( cw.getString(a.getClassIndex()).equals("Lcom/antlersoft/android/db/TableInterface;"))
					{
						result=true;
						readTableInterface(cw, a, td);
						// Look through methods for field definitions
						for ( MethodInfo mi : cw.getMethods())
						{
							RuntimeVisibleAnnotationsAttribute methodAnnotations=(RuntimeVisibleAnnotationsAttribute)mi.getAttributeList().getAttributeByType( RuntimeInvisibleAnnotationsAttribute.typeString);
							if ( methodAnnotations!=null)
							{
								for ( Annotation methodAnnotation : methodAnnotations.getAnnotations())
								{
									if (cw.getString(methodAnnotation.getClassIndex()).equals("Lcom/antlersoft/android/db/FieldAccessor;"))
									{
										readFieldDefinition(cw,mi,methodAnnotation,td);
									}
								}
							}
						}
						// Finalize field definitions
						for ( FieldDefinition fd : td.fieldDefinitions)
						{
							fd.javaType=getJavaType(fd.javaTypeCode);
							if ( fd.bothRequired && ! (fd.getRequired && fd.putRequired)) {
								if ( fd.getRequired)
								{
									String baseName=fd.getName;
									if ( baseName.startsWith("get") && nextIsCap(baseName, 3))
										baseName=baseName.substring(3);
									else if ( baseName.startsWith("is") && nextIsCap(baseName, 2))
										baseName=baseName.substring(2);
									fd.putName="set"+baseName;
									fd.putRequired=true;
								}
								else
								{
									String baseName=fd.putName;
									if ( baseName.startsWith("set") && nextIsCap(baseName, 3))
										baseName=baseName.substring(3);
									fd.putName="get"+baseName;
									fd.getRequired=true;
								}								
							}
						}
						java.io.PrintWriter pw=sourceBase.getWriterForClass(td.packageName, td.implementingClass);
						td.generateClassDefinition(pw);
						sourceBase.doneWithWriter(pw);
						break;
					}
				}
			}
		}
		return result;
	}
	
	private boolean getElementValueBoolean( String name, ClassWriter cw, Annotation a, boolean defaultValue)
	{
		Object o=a.getElementValue(cw, name);
		if ( o!=null && o instanceof Integer)
		{
			return ((Integer)o).intValue()!=0;
		}
		return defaultValue;
	}
	
	private void readTableInterface(ClassWriter cw, Annotation a, TableDefinition td)
	{
		String internalName=cw.getInternalClassName(cw.getCurrentClassIndex());
		td.packageName=TypeParse.packageFromInternalClassName(internalName);
		td.interfaceName=TypeParse.convertFromInternalClassName(internalName).substring(td.packageName.length()>0 ? td.packageName.length()+1 : 0);
		
		td.name=a.getElementValueAsString(cw, "TableName");
		if ( td.name.length()==0)
			td.name=td.interfaceName;
		td.implementingClass=a.getElementValueAsString(cw, "ImplementingClassName");
		if ( td.implementingClass.length()==0)
			td.implementingClass="Gen_" + td.interfaceName;
		
		td.makeAbstract=getElementValueBoolean( "ImplementingIsAbstract", cw, a, true);
		td.makePublic=getElementValueBoolean( "ImplementingIsPublic", cw, a, true);
	}
	
	static enum MethodKind
	{
		PUT, GET, UNKNOWN
	}
	
	/**
	 * Confirm that next character after prefixLen doesn't continue a lowercase word
	 * @param s
	 * @param prefixLen
	 * @return
	 */
	private static boolean nextIsCap( String s, int prefixLen)
	{
		if ( s.length() > prefixLen ) {
			char c=s.charAt(prefixLen);
			if ( c<'a' || c>'z')
				return true;
		}
		return false;
	}
	
	private static String lowerFirst( String s, int prefixLen)
	{
		return s.substring(prefixLen,prefixLen+1).toLowerCase()+s.substring(prefixLen+1);
	}
	
	private void readFieldDefinition( ClassWriter cw, MethodInfo mi, Annotation a, TableDefinition td)
		throws SourceInterface.SIException
	{
		String fieldName=mi.getName();
		ArrayList<String> sig;
		try
		{
			sig=TypeParse.parseMethodType(mi.getType());
		}
		catch ( CodeCheckException cce)
		{
			throw new SourceInterface.SIException("Can't interpret internal method signature "+mi.getType(), cce);
		}
		MethodKind mk=MethodKind.UNKNOWN;
		String typeCode=null;
		if ( sig.size()==2 && sig.get(0).equals(TypeParse.ARG_VOID))
		{
			mk=MethodKind.PUT;
			typeCode=sig.get(1);
			if ( typeCode==TypeParse.ARG_OBJREF) {
				String t=mi.getType();
				typeCode=t.substring(t.indexOf('(')+1,t.indexOf(')'));
			}
		}
		if ( sig.size()==1 )
		{
			mk=MethodKind.GET;
			typeCode=sig.get(0);
			if ( typeCode==TypeParse.ARG_OBJREF) {
				String t=mi.getType();
				typeCode=t.substring(t.indexOf(')')+1);
			}
		}
		if ( typeCode==null)
			return;
		// We don't support array types
		if ( typeCode.startsWith(TypeParse.ARG_ARRAYREF))
			return;
			
		if ( fieldName.startsWith("get") && nextIsCap( fieldName, 3))
		{
			if ( mk!=MethodKind.GET)
				return;
			fieldName=lowerFirst(fieldName,3);
		}
		else if ( fieldName.startsWith("is") && nextIsCap( fieldName, 2))
		{
			if ( mk != MethodKind.GET)
				return;
			fieldName=lowerFirst(fieldName,2);
		}
		else if ( fieldName.startsWith("set") && nextIsCap(fieldName,3))
		{
			if (mk != MethodKind.PUT)
				return;
			fieldName = lowerFirst(fieldName,3);
		}
		String specifiedName=a.getElementValueAsString(cw, "Name");
		if ( specifiedName.length()>0)
			fieldName=specifiedName;
		String columnName=fieldName.toUpperCase();
		if (columnName.equals("_ID"))
		{
			columnName = "_id";
		}
		FieldDefinition fd=null;
		for ( FieldDefinition existing : td.fieldDefinitions)
		{
			if ( existing.columnName.equals(columnName))
			{
				fd=existing;
				break;
			}
		}
		if ( fd==null )
		{
			fd=new FieldDefinition();
			fd.columnName=columnName;
			td.fieldDefinitions.add(fd);
		}
		fd.name=fieldName;
		if ( fd.javaTypeCode!=null && ! fd.javaTypeCode.equals(typeCode))
			return;
		fd.bothRequired=getElementValueBoolean("Both",cw,a,true);
		if (fd.javaTypeCode == null)
			fd.javaTypeCode = typeCode;
		String fieldTypeString = a.getElementValueAsString(cw, "Type");
		if ( fieldTypeString.length()==0)
		{
			if ( fd.type==null)
				fd.type=FieldType.DEFAULT;
		}
		else
		{
			fd.type=FieldType.valueOf(fieldTypeString);
		}
		if ( mk==MethodKind.GET )
		{
			fd.getRequired = true;
			fd.getName = mi.getName();
		}
		else
		{
			fd.putRequired = true;
			fd.putName = mi.getName();
		}
		if ( fd.type==FieldType.DEFAULT)
		{
			if ( fd.javaTypeCode.equals("Ljava/lang/String;") || fd.javaTypeCode.equals(TypeParse.ARG_CHAR)) {
				fd.type=FieldType.TEXT;
			}
			else if ( fd.javaTypeCode.equals(TypeParse.ARG_DOUBLE) || fd.javaTypeCode.equals(TypeParse.ARG_FLOAT)) {
				fd.type=FieldType.REAL;
			}
			else if ( fd.javaTypeCode.equals(TypeParse.ARG_LONG) || fd.javaTypeCode.equals(TypeParse.ARG_INT) ||
						fd.javaTypeCode.equals(TypeParse.ARG_SHORT) || fd.javaTypeCode.equals(TypeParse.ARG_BOOLEAN) ||
						fd.javaTypeCode.equals(TypeParse.ARG_BYTE)) {
				fd.type=FieldType.INTEGER;
				if ( columnName.equals("_id"))
					fd.type=FieldType.INTEGER_PRIMARY_KEY;
			}
			else
				fd.type=FieldType.BLOB;
		}
		fd.nullable=getElementValueBoolean("Nullable", cw, a, true);
		fd.defaultValue=null;
		if (a.getElementValue(cw, "DefaultValue")!=null)
		{
			fd.defaultValue=a.getElementValueAsString(cw,"DefaultValue");
		}
		String visibility=a.getElementValueAsString(cw, "Visbility");
		if ( visibility.length()==0)
			fd.visibility=FieldVisibility.PRIVATE;
		else
			fd.visibility=FieldVisibility.valueOf(a.getElementValueAsString(cw, "Visbility"));
	}
	
	private static String getJavaType(String javaTypeCode)
	{
		String javaType = null;
		for ( String[] pair : CODE_TO_JAVA_TYPE)
		{
			if ( pair[0].equals(javaTypeCode)) {
				javaType=pair[1];
				break;
			}
		}
		if ( javaType==null)
		{
			if (javaTypeCode.startsWith( TypeParse.ARG_OBJREF))	{
				javaType=TypeParse.convertFromInternalClassName(javaTypeCode.substring(1, javaTypeCode.length() - 1));
			}
		}
		return javaType;
	}
}
