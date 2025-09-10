/**
 * Copyright (c) 2008 Michael A. MacDonald
 */
package com.antlersoft.classwriter;

import java.io.DataInputStream;
import java.io.IOException;

/**
 * The set of annotations on a method, field or class that are invisible at runtime.
 * Exactly the same as RuntimeVisibleAnnotations, except for the type string.
 * @author Michael A. MacDonald
 *
 */
public class RuntimeInvisibleAnnotationsAttribute extends
		RuntimeVisibleAnnotationsAttribute {
	public final static String typeString="RuntimeInvisibleAnnotations";

	/**
	 * @param classStream
	 * @throws IOException
	 */
	public RuntimeInvisibleAnnotationsAttribute(DataInputStream classStream)
			throws IOException {
		super(classStream);
	}

	/* (non-Javadoc)
	 * @see com.antlersoft.classwriter.RuntimeVisibleAnnotationsAttribute#getTypeString()
	 */
	@Override
	public String getTypeString() {
		return typeString;
	}
}
