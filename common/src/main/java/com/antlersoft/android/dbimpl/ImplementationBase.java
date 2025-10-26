/**
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.antlersoft.android.dbimpl;

import android.content.ContentValues;
import android.database.Cursor;
import net.sqlcipher.database.SQLiteDatabase;

import java.util.Collection;

/**
 * @author Michael A. MacDonald
 *
 */
public abstract class ImplementationBase {
	/**
	 * Populate this instance from a cursor
	 * @param cursor
	 * @param columnIndices
	 */
	public abstract void Gen_populate(Cursor cursor,int[] columnIndices);
	
	public abstract void Gen_populate(ContentValues values);
	
	public abstract ContentValues Gen_getValues();

	/**
	 * Return the name of this table
	 * @return Name of the table that holds instances of this class
	 */
	public abstract String Gen_tableName();
	
	/**
     * Return an array that gives the column index in the cursor for each field defined
     * @param cursor Database cursor over some columns, possibly including this table
     * @return array of column indices; -1 if the column with that id is not in cursor
     */
	public abstract int[] Gen_columnIndices(Cursor cursor);
	
	public static <E extends ImplementationBase> void Gen_populateFromCursor(
			Cursor c,
			Collection<E> collection,
			NewInstance<E> instanceGenerator
			)
	{
		if ( c.moveToFirst())
		{
			E instance=instanceGenerator.get();
			int[] columnIndices=instance.Gen_columnIndices(c);
			do
			{
				if (instance == null)
				{
					instance=instanceGenerator.get();
				}
				instance.Gen_populate(c, columnIndices);
				collection.add(instance);
				instance=null;
			}
			while (c.moveToNext());
		}
	}
	
	public static <E extends ImplementationBase> void getAll(
			SQLiteDatabase database,
			String tableName,
			Collection<E> collection,
			NewInstance<E> instanceGenerator)
	{
		Cursor c = database.query(tableName,null,null,null,null,null,null);
		try
		{
			Gen_populateFromCursor(c,collection,instanceGenerator);
		}
		finally
		{
			c.close();
		}
	}
}
