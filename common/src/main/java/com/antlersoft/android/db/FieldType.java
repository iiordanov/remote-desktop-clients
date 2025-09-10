/**
 * Copyright (C) 2008 Michael A. MacDonald
 */
package com.antlersoft.android.db;

/**
 * @author Michael A. MacDonald
 *
 */
public enum FieldType {
	INTEGER_PRIMARY_KEY,
	INTEGER,
	TEXT,
	BLOB,
	REAL,
	// Derive type from type of accessor
	DEFAULT
}
