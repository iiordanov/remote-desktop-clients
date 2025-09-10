/**
 * Copyright (c) 2008 Michael A. MacDonald
 */
package com.antlersoft.classwriter;

import java.util.Collection;

/**
 * Interface implemented by all the parts of an AnnotationAttribute to facilitate
 * gathering the information needed by BBQ
 * 
 * @author Michael A. MacDonald
 *
 */
public interface AnnotationInfo {
	/**
	 * Collect information about the annotations and strings referenced within this object
	 * @param container Class that contains this object
	 * @param annotations Annotations nested within this object are added to this collection
	 * @param strings Strings referenced within this object are added to this collection
	 */
	public void gatherAnnotationInfo(
			ClassWriter container,
			Collection<Annotation> annotations,
			Collection<String> strings);
}
