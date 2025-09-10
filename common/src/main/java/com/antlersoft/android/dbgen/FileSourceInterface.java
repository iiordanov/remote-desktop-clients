/**
 * Copyright (C) 2008 Michael A. MacDonald
 */
package com.antlersoft.android.dbgen;

import java.util.StringTokenizer;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

import com.antlersoft.classwriter.ClassWriter;

/**
 * An interface to a source tree based on the java.io.File filesystem abstraction
 * @author Michael A. MacDonald
 *
 */
public class FileSourceInterface implements SourceInterface {
	
	private File base;
	/**
	 * 
	 * @param sourceBase Directory defining top of source tree, where top-level source files or packages dwell
	 */
	public FileSourceInterface( File sourceBase)
	{
		base=sourceBase;
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.android.dbgen.SourceInterface#doneWithWriter()
	 */
	public void doneWithWriter( PrintWriter w) throws IOException, SIException {
		w.close();
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.android.dbgen.SourceInterface#getWriterForClass(java.lang.String, java.lang.String)
	 */
	public PrintWriter getWriterForClass(String packageName, String className)
			throws IOException, SIException {
		File packageDir=base;
		for ( StringTokenizer st=new StringTokenizer(packageName,"."); st.hasMoreTokens();)
		{
			packageDir=new File( packageDir, st.nextToken());
		}
		packageDir.mkdirs();
		return new PrintWriter( new FileWriter( new File( packageDir, className+".java")));
	}
	
	public static void main( String[] argv) throws Exception
	{
		File base=new File(argv[0]);
		ClassWriter cw=new ClassWriter();
		cw.readClass(new java.io.FileInputStream(argv[1]));
		new SourceFileGenerator( new FileSourceInterface(base)).generate(cw);
	}
}
