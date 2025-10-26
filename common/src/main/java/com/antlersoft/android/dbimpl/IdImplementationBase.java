/**
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.antlersoft.android.dbimpl;

import android.content.ContentValues;
import android.database.Cursor;
import net.sqlcipher.database.SQLiteDatabase;

/**
 * Specialization of ImplementationBase for tables that have a long
 * primary key named _Id; this allows easy general purpose save and update
 * 
 * @author Michael A. MacDonald
 *
 */
public abstract class IdImplementationBase extends ImplementationBase {
	public abstract long get_Id();
	public abstract void set_Id(long id);
	
	/**
	 * Return the same ContentValues object with _ID field removed
	 * @param cv
	 * @return
	 */
	private static ContentValues removeId(ContentValues cv) {
		cv.remove("_id");
		return cv;
	}
	
	/**
	 * Populates this object with the contents of the row in the table for this class
	 * with the specified id
	 * @param db Database containing table for this class
	 * @param id Id of the row to read (value of the _id column)
	 * @return True if the row was found and read; false otherwise
	 */
	public boolean Gen_read(SQLiteDatabase db, long id) {
		Cursor c = db.query(Gen_tableName(), null, "_id = ?", new String[] { Long.toString(id) }, null, null, null);
		boolean result = false;
		
		if (c.moveToFirst())
		{
			Gen_populate(c, Gen_columnIndices(c));
			result = true;
		}
		
		c.close();
		return result;
	}
	
	/**
	 * Insert a row in the table with the values of this instance 
	 * @param db
	 * @return true if the row was inserted, false otherwise
	 */
	public boolean Gen_insert(SQLiteDatabase db) {
		long id=db.insert(Gen_tableName(),null,removeId(Gen_getValues()));
		if (id!= -1)
		{
			set_Id(id);
			return true;
		}
		return false;
	}
	
	/**
	 * Delete the row in the table corresponding to this instance
	 * @param db
	 * @return Number of rows deleted
	 */
	public int Gen_delete(SQLiteDatabase db) {
		return db.delete(Gen_tableName(), "_id = ?", new String[] { Long.toString(get_Id()) });
	}
	
	/**
	 * Update the row in the table corresponding to this instance with the values of the instance
	 * @param db
	 * @return Number of rows updated
	 */
	public int Gen_update(SQLiteDatabase db) {
		return db.update(Gen_tableName(), removeId(Gen_getValues()), "_id = ?", new String[] { Long.toString(get_Id()) });
	}
}
