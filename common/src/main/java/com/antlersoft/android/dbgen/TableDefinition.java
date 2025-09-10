/**
 * Copyright (C) 2008 Michael A. MacDonald
 */
package com.antlersoft.android.dbgen;

import java.io.PrintWriter;
import java.text.MessageFormat;
import java.util.ArrayList;

import com.antlersoft.android.db.FieldType;
import com.antlersoft.android.db.FieldVisibility;

import com.antlersoft.classwriter.TypeParse;

/**
 * Table properties as defined by annotations on interface class
 * @author Michael A. MacDonald
 *
 */
class TableDefinition {
	String packageName;
	String name;
	String implementingClass;
	String interfaceName;
	boolean makeAbstract;
	boolean makePublic;
	
	ArrayList<FieldDefinition> fieldDefinitions;
	
	TableDefinition(String name)
	{
		this.name=name;
		fieldDefinitions=new ArrayList<FieldDefinition>();
	}
	
	TableDefinition()
	{
		this("");
	}
	
	class Indenter
	{
		private PrintWriter pw;
		int indentation;
		/** True if last printed was a new line */
		private boolean endOfLine;
		
		Indenter(PrintWriter pw) {
			this.pw=pw;
			endOfLine=true;
		}
		
		void indent()
		{
			if ( ! endOfLine) {
				pw.println();
			}
			for ( int i=0; i<indentation; ++i)
			{
				pw.print("    ");
			}
			endOfLine=false;
		}

		/** Print a closing brace, adjusting indentation */
		void closeBrace()
		{
			indentation--;
			indent();
			pw.println("}");
			endOfLine=true;
		}
		
		/**
		 * Print a line with indentation; adjust indentation for opening brace
		 */
		void iprintln(String line)
		{
			indent();
			pw.println(line);
			endOfLine=true;
			if ( line.endsWith("{")) {
				indentation++;
			}
		}
		
		/**
		 * Print a line with indentation, prepending public if makePublic flag true
		 * @param line
		 */
		void ivprintln(String line)
		{
			indent();
			if ( makePublic ) { pw.print("public "); }
			pw.println(line);
			endOfLine=true;
			if ( line.endsWith("{")) {
				indentation++;
			}
		}
		
		void nl() {
			if ( ! endOfLine)
				pw.println();
			pw.println();
		}
	}

	private static String TABLE_NAME_SYMBOL = "GEN_TABLE_NAME";
	private static String FIELD_COUNT_SYMBOL = "GEN_COUNT";
	
	private static String idSymbol(FieldDefinition fd)
	{
		return "GEN_ID_"+fd.columnName.toUpperCase();
	}
	
	private static String nameSymbol(FieldDefinition fd)
	{
		return "GEN_FIELD_"+fd.columnName.toUpperCase();
	}
	
	private static String getObjectType(FieldDefinition fd) {
		String objectType = fd.javaType.substring(0,1).toUpperCase()+fd.javaType.substring(1);
		if ( objectType.equals("Int")) objectType="Integer";
		return objectType;
	}
	
	private boolean hasId()
	{
		for (FieldDefinition fd : fieldDefinitions)
		{
			if (fd.columnName.equals("_id") && fd.javaType.equals("long"))
				return true;
		}
		return false;
	}
	
	/**
	 * Escape a string so it can appear within Oracle single quotes
	 * @param f string to escape
	 * @return string with any ' characters doubled
	 */
	private static String escapeDBString(String f)
	{
		StringBuilder b=new StringBuilder();
		int l = f.length();
		for (int i=0; i<l; i++)
		{
			char c = f.charAt(i);
			if (c == '\'')
			{
				b.append(c);
			}
			b.append(c);
		}
		return b.toString();
	}
	
	/**
	 * Escape a string so it can appear within a double quoted string in Java source code
	 * @param f string to escape
	 * @return string with any " characters doubled and control characters converted to
	 * octal escapes
	 */
	private static String escapeJavaString(String f)
	{
		StringBuilder b=new StringBuilder();
		int l = f.length();
		for (int i=0; i<l; i++)
		{
			char c = f.charAt(i);
			if (c=='"')
			{
				b.append("\\\"");
			}
			else if (Character.getType(c)==Character.CONTROL)
			{
				b.append('\\');
				b.append(Integer.toOctalString(c));
			}
			else
			{
				b.append(c);
			}
		}
		return b.toString();
	}

	/**
	 * Return a string representing the default value of the field if one was provided
	 * @param fd
	 * @return
	 * @throws SourceInterface.SIException
	 */
	private static String defaultValueString(FieldDefinition fd)
	throws SourceInterface.SIException
	{
		StringBuilder result=new StringBuilder();
		if (fd.defaultValue!=null)
		{
			switch (fd.type)
			{
			case BLOB :
			case TEXT :
				result.append(" DEFAULT ");
				result.append('\'');
			    result.append(escapeJavaString(escapeDBString(fd.defaultValue)));
			    result.append('\'');
			    break;
			case INTEGER :
				if (fd.javaType.equals("boolean"))
				{
					result.append(" DEFAULT ");
					result.append(fd.defaultValue.equals("true") ? 1 : 0);
					break;
				}
				// Intentionally fall through for non-boolean case
			case REAL :
				try
				{
					double v = Double.parseDouble(fd.defaultValue);
					result.append(" DEFAULT ");
					result.append(v);
				}
				catch (NumberFormatException nfe)
				{
					throw new SourceInterface.SIException(fd.defaultValue + " could not be interpreted as a number");
				}
			default :
				throw new SourceInterface.SIException("Inappropriate field type for getting default value "+fd.type.toString());
			}
		}
		return result.toString();
	}
	
	void generateClassDefinition( PrintWriter pw)
	throws SourceInterface.SIException
	{
		Indenter id=new Indenter(pw);
		pw.println( MessageFormat.format( "// This class was generated from {0}.{1} by a tool", packageName, interfaceName));
		pw.println( "// Do not edit this file directly! PLX THX");
		pw.println( MessageFormat.format( "package {0};", packageName));
		id.nl();
		id.ivprintln( MessageFormat.format( "{2}class {0} extends com.antlersoft.android.dbimpl.{3}ImplementationBase implements {1} '{'",
				implementingClass, interfaceName, makeAbstract ? "abstract " : "", hasId() ? "Id" : ""));
		id.nl();
		id.ivprintln( MessageFormat.format( "static final String {0} = \"{1}\";", TABLE_NAME_SYMBOL, name.toUpperCase()));
		id.ivprintln( MessageFormat.format( "static final int {0} = {1};", FIELD_COUNT_SYMBOL, fieldDefinitions.size()));
		id.nl();
		
		// Write constants for field names and ids
		id.iprintln( "// Field constants");
		for ( int i=0; i<fieldDefinitions.size(); i++)
		{
			FieldDefinition fd=fieldDefinitions.get(i);
			id.ivprintln(MessageFormat.format("static final String {0} = \"{1}\";", nameSymbol(fd), fd.columnName));
			id.ivprintln(MessageFormat.format("static final int {0} = {1};", idSymbol(fd), i));
		}
		id.nl();
		
		// String for creating the table
		id.iprintln("// SQL Command for creating the table");
		id.ivprintln(MessageFormat.format("static String GEN_CREATE = \"CREATE TABLE {0} (\" +", name.toUpperCase()));
		for (int i = 0; i < fieldDefinitions.size(); ++i) {
			FieldDefinition fd = fieldDefinitions.get(i);
			String type = fd.type.toString();
			if (fd.type == FieldType.INTEGER_PRIMARY_KEY) {
				type = "INTEGER PRIMARY KEY AUTOINCREMENT";
			} else {
				if (! fd.nullable)
				{
					type = type + " NOT NULL";
				}
				type = type + defaultValueString(fd);
			}
			id.iprintln(MessageFormat.format("\"{0} {1}{2}\" +", fd.columnName, type, i == fieldDefinitions.size()-1 ? "" : ","));
		}
		id.iprintln("\")\";");
		
		id.nl();
		id.iprintln("// Members corresponding to defined fields");
		// Create variables for fields
		for ( FieldDefinition fd : fieldDefinitions)
		{
			id.indent();
			if ( fd.visibility != FieldVisibility.DEFAULT)
			{
				pw.print( fd.visibility.toString().toLowerCase());
				pw.print(' ');
			}
			pw.print( MessageFormat.format("{0} gen_{1};", fd.javaType, fd.name));
		}
		
		if (! makeAbstract) {
			id.nl();
			id.ivprintln(MessageFormat.format("static final com.antlersoft.android.dbimpl.NewInstance<{0}> GEN_NEW = new com.antlersoft.android.dbimpl.NewInstance<{0}>() '{'", implementingClass));
			id.iprintln(MessageFormat.format("public {0} get() '{'", implementingClass));
			id.iprintln(MessageFormat.format("return new {0}();", implementingClass));
			id.closeBrace();
			id.closeBrace();
			id.iprintln(";");
		}
		
		id.nl();
		id.iprintln(MessageFormat.format("public String Gen_tableName() '{' return {0}; }",TABLE_NAME_SYMBOL));
		
		id.nl();
		// Create accessors for fields
		id.iprintln( "// Field accessors");
		for ( FieldDefinition fd : fieldDefinitions)
		{
			if ( fd.getRequired)
			{
				id.iprintln( MessageFormat.format("public {0} {1}() '{' return gen_{2}; '}'", fd.javaType, fd.getName, fd.name));
			}
			if ( fd.putRequired)
			{
				id.iprintln( MessageFormat.format("public void {0}({1} arg_{2}) '{' gen_{2} = arg_{2}; '}'", fd.putName, fd.javaType, fd.name));
			}
		}
		id.nl();
		id.iprintln( "public android.content.ContentValues Gen_getValues() {");
		id.iprintln( "android.content.ContentValues values=new android.content.ContentValues();");
		for ( FieldDefinition fd : fieldDefinitions)
		{
			String stringValue = "this.gen_" + fd.name; 
			if ( ! fd.javaTypeCode.equals("Ljava/lang/String;"))
			{
				if ( fd.javaTypeCode.startsWith(TypeParse.ARG_OBJREF))
					stringValue = stringValue + ".toString()";
				else if ( fd.javaTypeCode.equals(TypeParse.ARG_BOOLEAN))
				{
					stringValue = MessageFormat.format("({0} ? \"1\" : \"0\")", stringValue);
				}
				else
				{
					stringValue = MessageFormat.format("{0}.toString({1})", getObjectType(fd), stringValue);
				}
			}
			id.iprintln(MessageFormat.format("values.put({0},{1});", nameSymbol(fd), stringValue));
		}
		id.iprintln("return values;");
		id.closeBrace();
		
		id.nl();
		id.iprintln( "/**");
		id.iprintln(" * Return an array that gives the column index in the cursor for each field defined");
		id.iprintln(" * @param cursor Database cursor over some columns, possibly including this table");
		id.iprintln(" * @return array of column indices; -1 if the column with that id is not in cursor");
		id.iprintln(" */");
		id.ivprintln("int[] Gen_columnIndices(android.database.Cursor cursor) {");
		id.iprintln(MessageFormat.format("int[] result=new int[{0}];",FIELD_COUNT_SYMBOL));
		for (int i=0; i<fieldDefinitions.size(); i++)
		{
			FieldDefinition fd=fieldDefinitions.get(i);
			id.iprintln(MessageFormat.format("result[{0}] = cursor.getColumnIndex({1});", i, nameSymbol(fd)));
			if (i == 0 && nameSymbol(fd).equals("GEN_FIELD__ID"))
			{
				id.iprintln("// Make compatible with database generated by older version of plugin with uppercase column name");
				id.iprintln("if (result[0] == -1) {");
				id.iprintln("result[0] = cursor.getColumnIndex(\"_ID\");");
				id.closeBrace();
			}
		}
		id.iprintln("return result;");
		id.closeBrace();
		
		id.nl();
		id.iprintln("/**");
		id.iprintln(" * Populate one instance from a cursor ");
		id.iprintln(" */" );
		id.ivprintln("void Gen_populate(android.database.Cursor cursor,int[] columnIndices) {");
		for (int i = 0; i < fieldDefinitions.size(); i++)
		{
			FieldDefinition fd=fieldDefinitions.get(i);
			id.iprintln(MessageFormat.format("if ( columnIndices[{0}] >= 0 && ! cursor.isNull(columnIndices[{0}])) '{'", idSymbol(fd)));
			switch (fd.type){
			case INTEGER :
			case INTEGER_PRIMARY_KEY :
				if (fd.javaType.equals("boolean")) {
					id.iprintln(MessageFormat.format("gen_{0} = (cursor.getInt(columnIndices[{1}]) != 0);",fd.name,idSymbol(fd)));
				} else if (fd.javaType.equals("long")) {
					id.iprintln(MessageFormat.format("gen_{0} = cursor.getLong(columnIndices[{1}]);",fd.name,idSymbol(fd)));					
				} else {
					id.iprintln(MessageFormat.format("gen_{0} = ({1})cursor.getInt(columnIndices[{2}]);",fd.name,fd.javaType,idSymbol(fd)));
				}
				break;
			case REAL :
				if (fd.javaType.equals("float")) {
					id.iprintln(MessageFormat.format("gen_{0} = cursor.getFloat(columnIndices[{1}]);", fd.name, idSymbol(fd)));
				} else {
					id.iprintln(MessageFormat.format("gen_{0} = cursor.getDouble(columnIndices[{1}]);", fd.name, idSymbol(fd)));
				}
				break;
			case TEXT :
				if (fd.javaType.equals("char")) {
					id.iprintln(MessageFormat.format("gen_{0} = cursor.getString(columnIndices[{1}]).charAt(0);", fd.name, idSymbol(fd)));
				} else {
					id.iprintln(MessageFormat.format("gen_{0} = cursor.getString(columnIndices[{1}]);", fd.name, idSymbol(fd)));
				}
			}
			id.closeBrace();
		}
		id.closeBrace();
		
		id.nl();
		id.iprintln("/**");
		id.iprintln(" * Populate one instance from a ContentValues ");
		id.iprintln(" */" );
		id.ivprintln("void Gen_populate(android.content.ContentValues values) {");
		for (int i = 0; i < fieldDefinitions.size(); i++)
		{
			FieldDefinition fd=fieldDefinitions.get(i);
			switch (fd.type){
			case INTEGER :
			case INTEGER_PRIMARY_KEY :
				if (fd.javaType.equals("boolean")) {
					id.iprintln(MessageFormat.format("gen_{0} = (values.getAsInteger({1}) != 0);",fd.name,nameSymbol(fd)));
				} else if (fd.javaType.equals("long")) {
					id.iprintln(MessageFormat.format("gen_{0} = values.getAsLong({1});",fd.name,nameSymbol(fd)));					
				} else {
					id.iprintln(MessageFormat.format("gen_{0} = ({2})values.getAsInteger({1});",fd.name,nameSymbol(fd),fd.javaType));
				}
				break;
			case REAL :
				if (fd.javaType.equals("float")) {
					id.iprintln(MessageFormat.format("gen_{0} = values.getAsFloat({1});", fd.name, nameSymbol(fd)));
				} else {
					id.iprintln(MessageFormat.format("gen_{0} = values.getAsDouble({1});", fd.name, nameSymbol(fd)));
				}
				break;
			case TEXT :
				if (fd.javaType.equals("char")) {
					id.iprintln(MessageFormat.format("gen_{0} = values.getAsString({1}).charAt(0);", fd.name, nameSymbol(fd)));
				} else {
					id.iprintln(MessageFormat.format("gen_{0} = values.getAsString({1});", fd.name, nameSymbol(fd)));
				}
			}
		}
		id.closeBrace();
		// End of class
		id.closeBrace();
	}
}
