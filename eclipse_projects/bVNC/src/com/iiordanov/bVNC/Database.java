/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC;

import java.io.File;

import javax.crypto.NullCipher;

import android.content.Context;
import net.sqlcipher.database.SQLiteDatabase;
import net.sqlcipher.database.SQLiteOpenHelper;
import android.util.Log;

/**
 * @author Michael A. MacDonald
 *
 */
public class Database extends SQLiteOpenHelper {
    static final int DBV_0_5_0 = 12;
    static final int DBV_1_2_0 = 20;
    static final int DBV_1_5_0 = 22;
    static final int DBV_1_6_0 = 291;
    static final int DBV_1_7_0 = 292;
    static final int DBV_1_8_0 = 293;
    static final int DBV_1_9_0 = 308;
    static final int DBV_2_0_0 = 309;
    static final int DBV_2_1_0 = 329;
    static final int DBV_2_1_1 = 335;
    static final int DBV_2_1_2 = 336;
    static final int DBV_2_1_3 = 360;
    static final int DBV_2_1_4 = 367;
    static final int CURRVERS = DBV_2_1_4;
    private static String dbName = "VncDatabase";
    private static String password = "";
    
    public final static String TAG = Database.class.toString();
    
    Database(Context context) {
        super(context, dbName, null, DBV_2_1_4);
        SQLiteDatabase.loadLibs(context);
    }

    /* (non-Javadoc)
     * @see net.sqlcipher.database.SQLiteOpenHelper#onCreate(net.sqlcipher.database.SQLiteDatabase)
     */
    @Override
    public void onCreate(SQLiteDatabase db) {
        
        db.execSQL(AbstractConnectionBean.GEN_CREATE);
        db.execSQL(MostRecentBean.GEN_CREATE);
        db.execSQL(MetaList.GEN_CREATE);
        db.execSQL(AbstractMetaKeyBean.GEN_CREATE);
        db.execSQL(SentTextBean.GEN_CREATE);
        
        db.execSQL("INSERT INTO " + MetaList.GEN_TABLE_NAME + " VALUES ( 1, 'DEFAULT')");
    }
    
    public SQLiteDatabase getWritableDatabase() {
        return getWritableDatabase(Database.password);
    }
    
    public SQLiteDatabase getReadableDatabase() {
        return getReadableDatabase(Database.password);
    }
    
    public static void setPassword (String newPassword) {
        Database.password = newPassword;
    }
    
    public void changeDatabasePassword (String newPassword) {
        // Get readable database with old password
        SQLiteDatabase db = getReadableDatabase(Database.password);
        int version = db.getVersion();
        String pathToDb = db.getPath();
        deleteTempDatabase(pathToDb);
        String newFormat = null;
        boolean swaptemp = false;

        // If the previous key is an empty string, we must encrypt a plaintext DB.
        if (Database.password.equals("")) {
            Log.i(TAG, "Previous database unencrypted, encrypting.");
            newFormat = "encrypted";
            swaptemp = true;
        // If the previous key is not an empty string, then we must rekey an existing DB.
        } else if (newPassword.equals("")) {
            Log.i(TAG, "Previous database encrypted, decrypting.");
            newFormat = "plaintext";
            swaptemp = true;
        // If both the previous and new password are non-empty, we are rekeying the DB.
        } else {
            Log.i(TAG, "Previous database encrypted, rekeying.");
            db.rawExecSQL(String.format("PRAGMA key = '%s'", Database.password));
            Database.setPassword(newPassword);
            // Rekey the database
            db.rawExecSQL(String.format("PRAGMA rekey = '%s'", Database.password));
        }
        
        if (swaptemp == true) {
            // Write out the database in the new format.
            Database.setPassword(newPassword);
            db.rawExecSQL(String.format("ATTACH DATABASE '%s' AS %s KEY '%s'", db.getPath()+"-temp", newFormat, Database.password));
            db.rawExecSQL(String.format("select sqlcipher_export('%s')", newFormat));
            db.rawExecSQL(String.format("DETACH DATABASE '%s'", newFormat));
            db.close();
            this.close();
            
            Log.i(TAG, "Done exporting to: " + newFormat);
            
            // Set version of database
            SQLiteDatabase tempDb = SQLiteDatabase.openOrCreateDatabase(db.getPath()+"-temp", Database.password, null);
            tempDb.setVersion(version);
            tempDb.close();
            
            // Move the temp database file over the true database file
            String tempDbPath = pathToDb + "-temp";
            new File(tempDbPath).renameTo(new File(pathToDb));
            tempDb = SQLiteDatabase.openOrCreateDatabase(db.getPath(), Database.password, null);
            tempDb.getVersion();
            tempDb.close();
        } else {
            db.close();
            this.close();
        }
    }
    
    private void deleteTempDatabase(String pathToDb) {
        String tempDbPath = pathToDb + "-temp";
        File temp = new File(tempDbPath);
        if (temp.exists()) {
            temp.delete();
        }
    }
    
    /* (non-Javadoc)
     * @see net.sqlcipher.database.SQLiteOpenHelper#onUpgrade(net.sqlcipher.database.SQLiteDatabase, int, int)
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
                    +AbstractConnectionBean.GEN_FIELD_AUTOXENABLED + " BOOLEAN DEFAULT FALSE");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_AUTOXTYPE + " INTEGER");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_AUTOXCOMMAND + " TEXT");
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
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_AUTOXRANDFILENM + " TEXT");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_AUTOXUNIXPW + " BOOLEAN DEFAULT FALSE");
            oldVersion = DBV_1_6_0;
        }

        if (oldVersion == DBV_1_6_0) {
            Log.i(TAG,"Doing upgrade from 291 to 292");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_AUTOXUNIXAUTH + " BOOLEAN DEFAULT FALSE");
            oldVersion = DBV_1_7_0;
        }

        if (oldVersion == DBV_1_7_0) {
            Log.i(TAG,"Doing upgrade from 292 to 293");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_EXTRAKEYSTOGGLETYPE + " INTEGER");
            oldVersion = DBV_1_8_0;
        }

        if (oldVersion == DBV_1_8_0) {
            Log.i(TAG,"Doing upgrade from 293 to 308");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_PREFENCODING + " INTEGER");
            oldVersion = DBV_1_9_0;
        }
        
        if (oldVersion == DBV_1_9_0) {
            Log.i(TAG,"Doing upgrade from 308 to 309");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_RDPDOMAIN + " TEXT");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_RDPRESTYPE + " INTEGER");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_RDPWIDTH + " INTEGER");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_RDPHEIGHT + " INTEGER");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_RDPCOLOR + " INTEGER");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_REMOTEFX + " BOOLEAN DEFAULT FALSE");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_DESKTOPBACKGROUND + " BOOLEAN DEFAULT FALSE");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_FONTSMOOTHING + " BOOLEAN DEFAULT FALSE");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_DESKTOPCOMPOSITION + " BOOLEAN DEFAULT FALSE");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_WINDOWCONTENTS + " BOOLEAN DEFAULT FALSE");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_MENUANIMATION + " BOOLEAN DEFAULT FALSE");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_VISUALSTYLES + " BOOLEAN DEFAULT FALSE");
            oldVersion = DBV_2_0_0;
        }
        
        if (oldVersion == DBV_2_0_0) {
            Log.i(TAG,"Doing upgrade from 309 to 329");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_CACERT + " TEXT");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_CACERTPATH + " TEXT");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_TLSPORT + " INTEGER");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_CERTSUBJECT + " TEXT");
            oldVersion = DBV_2_1_0;
        }
        
        if (oldVersion == DBV_2_1_0) {
            Log.i(TAG,"Doing upgrade from 329 to 335");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_ENABLESOUND + " BOOLEAN DEFAULT FALSE");
            oldVersion = DBV_2_1_1;
        }
        
        if (oldVersion == DBV_2_1_1) {
            Log.i(TAG,"Doing upgrade from 335 to 336");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_VIEWONLY + " BOOLEAN DEFAULT FALSE");
            oldVersion = DBV_2_1_2;
        }
        
        if (oldVersion == DBV_2_1_2) {
            Log.i(TAG,"Doing upgrade from 336 to 360");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_CONSOLEMODE + " BOOLEAN DEFAULT FALSE");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_REDIRECTSDCARD + " BOOLEAN DEFAULT FALSE");
            oldVersion = DBV_2_1_3;
        }
        
        if (oldVersion == DBV_2_1_3) {
            Log.i(TAG,"Doing upgrade from 360 to 367");
            db.execSQL("ALTER TABLE " + AbstractConnectionBean.GEN_TABLE_NAME + " ADD COLUMN "
                    +AbstractConnectionBean.GEN_FIELD_LAYOUTMAP + " TEXT DEFAULT 'English (US)'");
            oldVersion = DBV_2_1_4;
        }
    }

    public static String getPassword() {
        return password;
    }
}
