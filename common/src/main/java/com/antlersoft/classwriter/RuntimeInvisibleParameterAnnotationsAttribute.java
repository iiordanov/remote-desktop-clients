/**
 * Copyright (c) 2008 Michael A. MacDonald
 */
package com.antlersoft.classwriter;

import java.io.DataInputStream;
import java.io.IOException;

/**
 * @author Michael A. MacDonald
 *
 */
public class RuntimeInvisibleParameterAnnotationsAttribute extends
		RuntimeVisibleParameterAnnotationsAttribute {

	public final static String typeString="RuntimeInvisibleParameterAnnotations";

	/**
	 * @param classStream
	 * @throws IOException
	 */
	public RuntimeInvisibleParameterAnnotationsAttribute(
			DataInputStream classStream) throws IOException {
		super(classStream);
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.classwriter.Attribute#getTypeString()
	 */
	public String getTypeString() {
		return typeString;
	}
}
