/**
 * Copyright (c) 2007 Michael A. MacDonald
 */
package com.antlersoft.util;

import java.util.logging.Formatter;
import java.util.logging.LogRecord;

/**
 * Formatter that displays nothing but the LogRecord message
 * @author Michael A. MacDonald
 *
 */
public class MessageOnlyFormatter extends Formatter {


	/* (non-Javadoc)
	 * @see java.util.logging.Formatter#format(java.util.logging.LogRecord)
	 */
	public String format(LogRecord record) {
		return record.getMessage()+"\n";
	}

}
