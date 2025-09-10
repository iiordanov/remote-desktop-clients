/**
 * Copyright (C) 2008 Michael A. MacDonald
 */
package com.antlersoft.android.db;

import java.lang.annotation.*;

/**
 * Marks an accessor method as corresponding to a field in the table for this type
 * @author Michael A. MacDonald
 *
 */
@Retention(RetentionPolicy.CLASS)
@Target(ElementType.METHOD)
public @interface FieldAccessor {
	public String Name() default "";
	public FieldType Type() default FieldType.DEFAULT;
	public FieldVisibility Visibility() default FieldVisibility.PRIVATE;
	public boolean Nullable() default true;
	public String DefaultValue() default "";
	public boolean Both() default true;
}
