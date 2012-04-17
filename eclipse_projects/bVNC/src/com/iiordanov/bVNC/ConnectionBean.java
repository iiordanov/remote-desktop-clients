/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 */
package com.iiordanov.bVNC;

import android.content.ContentValues;
import android.database.sqlite.SQLiteDatabase;
import android.widget.ImageView.ScaleType;

import com.antlersoft.android.db.FieldAccessor;
import com.antlersoft.android.dbimpl.NewInstance;
import com.iiordanov.bVNC.COLORMODEL;
import com.iiordanov.bVNC.VncCanvasActivity;

import java.lang.Comparable;
import android.content.Context;

/**
 * @author Iordan Iordanov
 * @author Michael A. MacDonald
 *
 */
class ConnectionBean extends AbstractConnectionBean implements Comparable<ConnectionBean> {
	
	static Context c = null;
	
	static final NewInstance<ConnectionBean> newInstance=new NewInstance<ConnectionBean>() {
		public ConnectionBean get() { return new ConnectionBean(c); }
	};
	ConnectionBean(Context context)
	{
		set_Id(0);
		setAddress("");
		setPassword("");
		setKeepPassword(true);
		setNickname("");
		// TODO: Switch away from using a hard-coded numeric value for the connection type.
		setConnectionType(0);
		setSshServer("");
		setSshPort(22);
		setSshUser("");
		setSshPassword("");
		setKeepSshPassword(false);
		setSshPubKey("");
		setSshPrivKey("");
		setSshPassPhrase("");
		setUseSshPubKey(false);
		setSshHostKey("");
		setSshRemoteCommandOS(0);
		setSshRemoteCommand("");
		setSshRemoteCommandTimeout(5);
		setUseSshRemoteCommand(false);
		setUserName("");
		setPort(5900);
		setColorModel(COLORMODEL.C24bit.nameString());
		setScaleMode(ScaleType.MATRIX);
		setInputMode(VncCanvasActivity.TOUCH_ZOOM_MODE);
		setRepeaterId("");
		setMetaListId(1);
		c = context;
	}
	
	boolean isNew()
	{
		return get_Id()== 0;
	}
	
	void save(SQLiteDatabase database) {
		ContentValues values=Gen_getValues();
		values.remove(GEN_FIELD__ID);
		// Never save the SSH password.
		values.put(GEN_FIELD_SSHPASSWORD, "");
		if ( ! getKeepPassword()) {
			values.put(GEN_FIELD_PASSWORD, "");
		}
		if ( isNew()) {
			set_Id(database.insert(GEN_TABLE_NAME, null, values));
		} else {
			database.update(GEN_TABLE_NAME, values, GEN_FIELD__ID + " = ?", new String[] { Long.toString(get_Id()) });
		}
	}
	
	ScaleType getScaleMode()
	{
		return ScaleType.valueOf(getScaleModeAsString());
	}
	
	void setScaleMode(ScaleType value)
	{
		setScaleModeAsString(value.toString());
	}
	
	@Override
	public String toString() {
		if (isNew())
		{
			return c.getString(R.string.new_connection);
		}
		String result = new String("");
		
		// Add the nickname if it has been set.
		if (!getNickname().equals(""))
			result += getNickname()+":";
		
		// If this is an VNC over SSH connection, add the SSH server:port in parentheses
		// TODO: Switch away from numeric representation of position in list.
		if (getConnectionType() == 1)
			result += "(" + getSshServer() + ":" + getSshPort() + ")" + ":";

		// Add the VNC server and port.
		result += getAddress()+":"+getPort();
		return result;
	}

	/* (non-Javadoc)
	 * @see java.lang.Comparable#compareTo(java.lang.Object)
	 */
	@Override
	public int compareTo(ConnectionBean another) {
		int result = getNickname().compareTo(another.getNickname());
		if (result == 0) {
			result = getConnectionType() - another.getConnectionType();
		}		
		if (result == 0) {
			result = getAddress().compareTo(another.getAddress());
		}
		if ( result == 0) {
			result = getPort() - another.getPort();
		}
		if (result == 0) {
			result = getSshServer().compareTo(another.getSshServer());
		}
		if (result == 0) {
			result = getSshPort() - another.getSshPort();
		}
		return result;
	}
}
