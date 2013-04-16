/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
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
		setSshRemoteCommandType(0);
		setSshRemoteCommand("");
		setSshRemoteCommandTimeout(5);
		setAutoXType(0);
		setAutoXCommand("");
		setAutoXEnabled(false);
		setAutoXResType(0);
		setAutoXWidth(0);
		setAutoXHeight(0);
		setAutoXSessionProg("");
		setAutoXSessionType(0);
		setAutoXUnixpw(false);
		setAutoXUnixAuth(false);
		setAutoXRandFileNm("");
		setUseSshRemoteCommand(false);
		setUserName("");
		setRdpDomain("");
		setPort(5900);
		setColorModel(COLORMODEL.C24bit.nameString());
		setPrefEncoding(RfbProto.EncodingTight);
		setScaleMode(ScaleType.MATRIX);
		setInputMode(TouchMouseSwipePanInputHandler.TOUCH_ZOOM_MODE);
		setUseDpadAsArrows(false);
		setRotateDpad(false);
		setUsePortrait(false);
		setUseLocalCursor(false);
		setRepeaterId("");
		setExtraKeysToggleType(1);
		setMetaListId(1);
		c = context;
	}
	
	boolean isNew()
	{
		return get_Id()== 0;
	}
	
	synchronized void save(SQLiteDatabase database) {
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
