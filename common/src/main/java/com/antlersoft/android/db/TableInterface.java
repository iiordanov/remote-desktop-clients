/**
 * Copyright (C) 2008 Michael A. MacDonald
 */
package com.antlersoft.android.db;

import java.lang.annotation.*;

/**
 * Annotation assigned to an interface that will be used to define a table for the code generator
 * @author Michael A. MacDonald
 *
 */
@Retention(RetentionPolicy.CLASS)
@Target(ElementType.TYPE)
public @interface TableInterface {
	public String TableName() default "";
	public String ImplementingClassName() default "";
	public boolean ImplementingIsAbstract() default true;
	public boolean ImplementingIsPublic() default true;
}
