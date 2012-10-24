/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

/**
 * @author Michael A. MacDonald
 *
 */
class VncDatabase extends SQLiteOpenHelper {
	static final int DBV_0_5_0 = 12;
	static final int DBV_1_2_0 = 20;
	static final int DBV_1_5_0 = 22;
	static final int DBV_1_6_0 = 291;
	
	public final static String TAG = VncDatabase.class.toString();
	
	VncDatabase(Context context)
	{
		super(context, "VncDatabase", null, DBV_1_6_0);
	}

	/* (non-Javadoc)
	 * @see android.database.sqlite.SQLiteOpenHelper#onCreate(android.database.sqlite.SQLiteDatabase)
	 */
	@Override
	public void onCreate(SQLiteDatabase db) {
		db.execSQL(AbstractConnectionBean.GEN_CREATE);
		db.execSQL(MostRecentBean.GEN_CREATE);
		db.execSQL(MetaList.GEN_CREATE);
		db.execSQL(AbstractMetaKeyBean.GEN_CREATE);
		db.execSQL(SentTextBean.GEN_CREATE);
		
		db.execSQL("INSERT INTO "+MetaList.GEN_TABLE_NAME+" VALUES ( 1, 'DEFAULT')");
	}

	/* (non-Javadoc)
	 * @see android.database.sqlite.SQLiteOpenHelper#onUpgrade(android.database.sqlite.SQLiteDatabase, int, int)
	 */
	@Override
	public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
		if (oldVersion == DBV_0_5_0) {
			Log.i(TAG,"Doing upgrade from 12 to 20");
			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_CONNECTIONTYPE + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHSERVER + " TEXT");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHPORT + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHUSER + " TEXT");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHPASSWORD + " TEXT");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_KEEPSSHPASSWORD + " BOOLEAN DEFAULT FALSE");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHPUBKEY + " TEXT");
			
			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHPRIVKEY + " TEXT");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHPASSPHRASE + " TEXT");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_USESSHPUBKEY + " BOOLEAN DEFAULT FALSE");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHHOSTKEY + " TEXT");
			
			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHREMOTECOMMANDOS + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHREMOTECOMMAND + " TEXT");
			
			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHREMOTECOMMANDTIMEOUT + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_USESSHREMOTECOMMAND + " BOOLEAN DEFAULT FALSE");
			
			oldVersion = DBV_1_2_0;
		}
		
		if (oldVersion == DBV_1_2_0) {
			Log.i(TAG,"Doing upgrade from 20 to 22");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_USEDPADASARROWS + " BOOLEAN DEFAULT FALSE");
			
			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_ROTATEDPAD + " BOOLEAN DEFAULT FALSE");
			
			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_USEPORTRAIT + " BOOLEAN DEFAULT FALSE");
			
			oldVersion = DBV_1_5_0;
		}
		
		if (oldVersion == DBV_1_5_0) {
			Log.i(TAG,"Doing upgrade from 22 to 291");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_SSHREMOTECOMMANDTYPE + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_AUTOXRESTYPE + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_AUTOXWIDTH + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_AUTOXHEIGHT + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_AUTOXSESSIONTYPE + " INTEGER");

			db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
					+AbstractConnectionBean.GEN_FIELD_AUTOXSESSIONPROG + " TEXT");

			oldVersion = DBV_1_6_0;
		}
	}
}
