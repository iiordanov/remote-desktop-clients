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

import java.io.UnsupportedEncodingException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import net.sqlcipher.database.SQLiteDatabase;
import android.net.Uri;
import android.util.Log;
import android.widget.ImageView.ScaleType;

import com.antlersoft.android.dbimpl.NewInstance;
import com.iiordanov.bVNC.input.InputHandlerDirectSwipePan;
import com.iiordanov.bVNC.*;
import com.iiordanov.bVNC.input.InputHandlerTouchpad;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;

/**
 * @author Iordan Iordanov
 * @author Michael A. MacDonald
 * @author David Warden
 *
 */
public class ConnectionBean extends AbstractConnectionBean implements Comparable<ConnectionBean> {
    
	private static final String TAG = "ConnectionBean";
    static Context c = null;
    protected boolean m_isReadyForConnection = true; // saved connections are OK
    protected boolean m_saved = false;
    private int idHashAlgorithm;
    private String idHash;
    private String masterPassword;
    
    static final NewInstance<ConnectionBean> newInstance=new NewInstance<ConnectionBean>() {
        public ConnectionBean get() { return new ConnectionBean(c); }
    };
    ConnectionBean(Context context)
    {
        String inputMode = InputHandlerDirectSwipePan.ID;
        if (context != null) {
            inputMode = Utils.querySharedPreferenceString(context, Constants.defaultInputMethodTag,
                    InputHandlerDirectSwipePan.ID);
        } else {
            android.util.Log.e(TAG, "Failed to query default input method, context is null.");
        }
        set_Id(0);
        setAddress("");
        setPassword("");
        setKeepPassword(true);
        setNickname("");
        setConnectionType(Constants.CONN_TYPE_PLAIN);
        setSshServer("");
        setSshPort(Constants.DEFAULT_SSH_PORT);
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
        setPort(Constants.DEFAULT_PROTOCOL_PORT);
        setCaCert("");
        setCaCertPath("");
        setTlsPort(-1);
        setCertSubject("");
        setColorModel(COLORMODEL.C24bit.nameString());
        setPrefEncoding(RfbProto.EncodingTight);
        setScaleMode(ScaleType.MATRIX);
        setInputMode(inputMode);
        setUseDpadAsArrows(true);
        setRotateDpad(false);
        setUsePortrait(false);
        setUseLocalCursor(false);
        setRepeaterId("");
        setExtraKeysToggleType(1);
        setMetaListId(1);
        setRdpResType(0);
        setRdpWidth(0);
        setRdpHeight(0);
        setRdpColor(0);
        setRemoteFx(false);
        setDesktopBackground(false);
        setFontSmoothing(false);
        setDesktopComposition(false);
        setWindowContents(false);
        setMenuAnimation(false);
        setVisualStyles(false);
        setConsoleMode(false);
        setRedirectSdCard(false);
        setEnableSound(false);
        setEnableRecording(false);
        setRemoteSoundType(Constants.REMOTE_SOUND_ON_DEVICE);
        setViewOnly(false);
		setLayoutMap("English (US)");
        c = context;
        
        // These two are not saved in the database since we always save the cert data. 
        setIdHashAlgorithm(Constants.ID_HASH_SHA1);
        setIdHash("");
    }
    
    public int getIdHashAlgorithm() {
        return idHashAlgorithm;
    }

    public void setIdHashAlgorithm(int idHashAlgorithm) {
        this.idHashAlgorithm = idHashAlgorithm;
    }

    public String getIdHash() {
        return idHash;
    }

    public void setIdHash(String idHash) {
        this.idHash = idHash;
    }

    boolean isNew()
    {
        return get_Id()== 0;
    }
    
    public synchronized void save(SQLiteDatabase database) {
        ContentValues values = Gen_getValues();
        values.remove(GEN_FIELD__ID);
        // Never save the SSH password and passphrase.
        values.put(GEN_FIELD_SSHPASSWORD, "");
        values.put(GEN_FIELD_SSHPASSPHRASE, "");
        if (!getKeepPassword()) {
            values.put(GEN_FIELD_PASSWORD, "");
        }
        if (isNew()) {
            set_Id(database.insert(GEN_TABLE_NAME, null, values));
        } else {
            database.update(GEN_TABLE_NAME, values, GEN_FIELD__ID + " = ?", new String[] { Long.toString(get_Id()) });
        }
    }
    
    public boolean isReadyForConnection()
    {
    	return m_isReadyForConnection;
    }
    public boolean isSaved()
    {
    	return m_saved;
    }
    
    ScaleType getScaleMode()
    {
        return ScaleType.valueOf(getScaleModeAsString());
    }
    
    void setScaleMode(ScaleType value)
    {
        setScaleModeAsString(value.toString());
    }
    
    static ConnectionBean createLoadFromUri(Uri dataUri, Context ctx)
    {
        android.util.Log.d(TAG, "Creating connection from URI");
    	ConnectionBean connection = new ConnectionBean(ctx);
    	if (dataUri == null) return connection;
		Database database = new Database(ctx);
      	String host = dataUri.getHost();
      	
    	// Intent generated by connection shortcut widget
    	if (host != null && host.startsWith(Utils.getConnectionString(ctx))) {
            int port = 0;
            int idx = host.indexOf(':');

            if (idx != -1) {
                try {
                    port = Integer.parseInt(host.substring(idx + 1));
                }
                catch (NumberFormatException nfe) { }
                host = host.substring(0, idx);
            }
            
            if (connection.Gen_read(database.getReadableDatabase(), port))
            {
                MostRecentBean bean = getMostRecent(database.getReadableDatabase());
                if (bean != null)
                {
                    bean.setConnectionId(connection.get_Id());
                    bean.Gen_update(database.getWritableDatabase());
                    database.close();
                }
            }
            return connection;
    	}
    	
    	// search based on nickname
    	SQLiteDatabase queryDb = database.getReadableDatabase();
    	String connectionName = dataUri.getQueryParameter(Constants.PARAM_CONN_NAME);
    	Cursor nickCursor = null;
    	if (connectionName != null)
    		nickCursor = queryDb.query(GEN_TABLE_NAME, new String[] { GEN_FIELD__ID }, GEN_FIELD_NICKNAME  + " = ?", new String[] { connectionName }, null, null, null);
    	if (nickCursor != null && nickCursor.moveToFirst())
    	{
    		// there could be many values, so we will just pick one
    		Log.i(TAG, String.format(Locale.US, "Loding connection info from nickname: %s", connectionName));
    		connection.Gen_populate(nickCursor, connection.Gen_columnIndices(nickCursor));
    		nickCursor.close();
    		database.close();
    		return connection;
    	}
    	if (nickCursor != null)
    		nickCursor.close();
    	
    	// search based on hostname
    	Cursor hostCursor = null;
    	if (host != null)
    		hostCursor = queryDb.query(GEN_TABLE_NAME, new String[] { GEN_FIELD__ID }, GEN_FIELD_ADDRESS  + " = ?", new String[] { host }, null, null, null);
    	if (hostCursor != null && hostCursor.moveToFirst())
    	{
    		Log.i(TAG, String.format(Locale.US, "Loding connection info from hostname: %s", host));
    		connection.Gen_populate(hostCursor, connection.Gen_columnIndices(hostCursor));
    		hostCursor.close();
    		database.close();
    		return connection;
    	}
    	if (hostCursor != null)
    		hostCursor.close();
		database.close();
    	return connection;
    }
    
    void parseFromUri(Uri dataUri) {
    	Log.i(TAG, "Parsing VNC URI.");
    	if (dataUri == null) {
    		m_isReadyForConnection = false;
    		m_saved = true;
    		return;
    	}
    	
    	String host = dataUri.getHost();
    	if (host != null) {
    		setAddress(host);
    		
    		// by default, the connection name is the host name
    		String nickName = getNickname();
    		if (Utils.isNullOrEmptry(nickName)) {
    			setNickname(host);
    		}
    		
    		// default to use same host for ssh
    		if (Utils.isNullOrEmptry(getSshServer())) {
    		    setSshServer(host);
    		}
    	}
    	
    	final int PORT_NONE = -1;
        int port = dataUri.getPort();
        if (port != PORT_NONE && !isValidPort(port)) {
        	throw new IllegalArgumentException("The specified VNC port is not valid.");
        }
        setPort(port);
        
    	// handle legacy android-vnc-viewer parsing vnc://host:port/colormodel/password            
        List<String> path = dataUri.getPathSegments();
        if (path.size() >= 1) {
            setColorModel(path.get(0));
        }
        
        if (path.size() >= 2) {
            setPassword(path.get(1));
        }
        
    	// query based parameters
        String connectionName = dataUri.getQueryParameter(Constants.PARAM_CONN_NAME);
        
        if (connectionName != null) {
        	setNickname(connectionName);
        }
        
    	ArrayList<String> supportedUserParams = new ArrayList<String>() {{
    	    add(Constants.PARAM_RDP_USER); add(Constants.PARAM_SPICE_USER); add(Constants.PARAM_VNC_USER);
    	}};
    	for (String userParam : supportedUserParams) {
            String username = dataUri.getQueryParameter(userParam);
            if (username != null) {
                setUserName(username);
                break;
            }
        }
    	
        ArrayList<String> supportedPwdParams = new ArrayList<String>() {{
            add(Constants.PARAM_RDP_PWD); add(Constants.PARAM_SPICE_PWD); add(Constants.PARAM_VNC_PWD);
        }};
        for (String pwdParam : supportedPwdParams) {
            String password = dataUri.getQueryParameter(pwdParam);
            if (password != null) {
                setPassword(password);
                break;
            }
        }
        
    	setKeepPassword(false); // we should not store the password unless it is encrypted
    	
    	String securityTypeParam = dataUri.getQueryParameter(Constants.PARAM_SECTYPE);
    	int secType = 0; //invalid
    	if (securityTypeParam != null) {
    		secType = Integer.parseInt(securityTypeParam); // throw if invalid
    		switch (secType) {
    		case Constants.SECTYPE_NONE:
    		case Constants.SECTYPE_VNC:
    			setConnectionType(Constants.CONN_TYPE_PLAIN);
    			break;
    		case Constants.SECTYPE_INTEGRATED_SSH:
    			setConnectionType(Constants.CONN_TYPE_SSH);
    			break;
    		case Constants.SECTYPE_ULTRA:
    			setConnectionType(Constants.CONN_TYPE_ULTRAVNC);
    			break;
    		case Constants.SECTYPE_TLS:
    			setConnectionType(Constants.CONN_TYPE_ANONTLS);
    			break;
    		case Constants.SECTYPE_VENCRYPT:
    			setConnectionType(Constants.CONN_TYPE_VENCRYPT);
    			break;
    		case Constants.SECTYPE_TUNNEL:
    			setConnectionType(Constants.CONN_TYPE_STUNNEL);
    			break;
    		default:
    			throw new IllegalArgumentException("The specified security type is invalid or unsupported.");   
    		}
    	}
    	
    	// ssh parameters
    	String sshHost = dataUri.getQueryParameter(Constants.PARAM_SSH_HOST);
    	if (sshHost != null) {
    	    setSshServer(sshHost);
    	}
    	
    	String sshPortParam = dataUri.getQueryParameter(Constants.PARAM_SSH_PORT);
    	if (sshPortParam != null) {
    		int sshPort = Integer.parseInt(sshPortParam);
    		if (!isValidPort(sshPort))
        		throw new IllegalArgumentException("The specified SSH port is not valid.");
    		setSshPort(sshPort);
    	}
    	
    	String sshUser = dataUri.getQueryParameter(Constants.PARAM_SSH_USER);
    	if (sshUser != null) {
    	    setSshUser(sshUser);
    	}
    	
    	String sshPassword = dataUri.getQueryParameter(Constants.PARAM_SSH_PWD);
    	if (sshPassword != null) {
    	    setSshPassword(sshPassword);
    	}
    	
    	// security hashes
       	String idHashAlgParam = dataUri.getQueryParameter(Constants.PARAM_ID_HASH_ALG);
       	if (idHashAlgParam != null) {
       		int idHashAlg = Integer.parseInt(idHashAlgParam); // throw if invalid
       		switch (idHashAlg) {
       		case Constants.ID_HASH_MD5:
       		case Constants.ID_HASH_SHA1:
       		case Constants.ID_HASH_SHA256:
       			setIdHashAlgorithm(idHashAlg);
       			break;
       		default:
    			// we are given a bad parameter
       			throw new IllegalArgumentException("The specified hash algorithm is invalid or unsupported.");   
       		}
       	}
       	
    	String idHash = dataUri.getQueryParameter(Constants.PARAM_ID_HASH);
    	if (idHash != null) {
    	    setIdHash(idHash);
    	}
    	
    	String viewOnlyParam = dataUri.getQueryParameter(Constants.PARAM_VIEW_ONLY);
    	if (viewOnlyParam != null) setViewOnly(Boolean.parseBoolean(viewOnlyParam));
    	
    	String scaleModeParam = dataUri.getQueryParameter(Constants.PARAM_SCALE_MODE);
    	if (scaleModeParam != null) setScaleMode(ScaleType.valueOf(scaleModeParam));
    	
    	String extraKeysToggleParam = dataUri.getQueryParameter(Constants.PARAM_EXTRAKEYS_TOGGLE);
    	if (extraKeysToggleParam != null) setExtraKeysToggleType(Integer.parseInt(extraKeysToggleParam));
    	
    	// color model
    	String colorModelParam = dataUri.getQueryParameter(Constants.PARAM_COLORMODEL);
    	if (colorModelParam != null) {
    		int colorModel = Integer.parseInt(colorModelParam); // throw if invalid
    		switch (colorModel) {
    		case Constants.COLORMODEL_BLACK_AND_WHITE:
    			setColorModel(COLORMODEL.C2.nameString());
    			break;
    		case Constants.COLORMODEL_GREYSCALE:
    			setColorModel(COLORMODEL.C4.nameString());
    			break;
    		case Constants.COLORMODEL_8_COLORS:
    			setColorModel(COLORMODEL.C8.nameString());
    			break;
    		case Constants.COLORMODEL_64_COLORS:
    			setColorModel(COLORMODEL.C64.nameString());
    			break;
    		case Constants.COLORMODEL_256_COLORS:
    			setColorModel(COLORMODEL.C256.nameString());
    			break;
    			// use the best currently available model
    		case Constants.COLORMODEL_16BIT:
    			setColorModel(COLORMODEL.C24bit.nameString());
    			break;
    		case Constants.COLORMODEL_24BIT:
    			setColorModel(COLORMODEL.C24bit.nameString());
    			break;
    		case Constants.COLORMODEL_32BIT:
    			setColorModel(COLORMODEL.C24bit.nameString());
    			break;
    		default:
    			// we are given a bad parameter
    			throw new IllegalArgumentException("The specified color model is invalid or unsupported.");    
    		}
    	}
    	String saveConnectionParam = dataUri.getQueryParameter(Constants.PARAM_SAVE_CONN);
    	boolean saveConnection = true;
    	if (saveConnectionParam != null) {
    		saveConnection = Boolean.parseBoolean(saveConnectionParam); // throw if invalid
    	}
    	
    	// Parse a passed-in TLS port number.
        String tlsPortParam = dataUri.getQueryParameter(Constants.PARAM_TLS_PORT);
        if (tlsPortParam != null) {
            int tlsPort = Integer.parseInt(tlsPortParam);
            if (!isValidPort(tlsPort))
                throw new IllegalArgumentException("The specified TLS port is not valid.");
            setTlsPort(tlsPort);
        }
        
        // Parse a CA Cert path parameter
        String caCertPath = dataUri.getQueryParameter(Constants.PARAM_CACERT_PATH);
        if (caCertPath != null) {
            setCaCertPath(caCertPath);
        }
        
        // Parse a Cert subject
        String certSubject = dataUri.getQueryParameter(Constants.PARAM_CERT_SUBJECT);
        if (certSubject != null) {
            setCertSubject(certSubject);
        }
    	
        // Parse a keyboard layout parameter
        String keyboardLayout = dataUri.getQueryParameter(Constants.PARAM_KEYBOARD_LAYOUT);
        if(keyboardLayout != null) {
            setLayoutMap(keyboardLayout);
        }
		
    	// if we are going to save the connection, we will do so here
    	// it may make sense to confirm overwriting data but is probably unnecessary
    	if (saveConnection) {
    		Database database = new Database(c);
    		save(database.getWritableDatabase());
    		database.close();
    		m_saved = true;
    	}
    	
    	// we do not currently use API keys
    	
    	// check if we need to show data-entry screen
    	// it may be possible to prompt for data later
    	m_isReadyForConnection = true;
    	if (Utils.isNullOrEmptry(getAddress())) {
    		m_isReadyForConnection = false;
    		Log.i(TAG, "URI missing remote address.");
    	}
    	
    	int connType = getConnectionType();
    	if (secType == Constants.SECTYPE_VNC || connType == Constants.CONN_TYPE_STUNNEL
    			|| connType == Constants.CONN_TYPE_SSH) {
    		// we can infer a password is required
    		// while we could have implemented tunnel/ssh without one 
    		// the user can supply a blank value and the server will not
    		// request it and it is better to support the common case
    		if (Utils.isNullOrEmptry(getPassword())) {
    			m_isReadyForConnection = false;
    			Log.i(TAG, "URI missing VNC password.");
    		}
    	}
    	if (connType == Constants.CONN_TYPE_SSH) {
    		// the below should not occur
    		if (Utils.isNullOrEmptry(getSshServer()))
    			m_isReadyForConnection = false;
    		// we probably need either a username/password or a key
    		// however the main screen doesn't validate this
    	}
    	// some other types probably require a username/password 
    	// however main screen doesn't validate this
    }
    
    boolean isValidPort(int port) {
    	final int PORT_MAX = 65535;
    	if (port <= 0 || port > PORT_MAX)
    		return false;
    	return true;
    }
    
    @Override
    public String toString() {
        if (isNew()) {
            return c.getString(R.string.new_connection);
        }
        String result = new String("");
        
        // Add the nickname if it has been set.
        if (!getNickname().equals("")) {
            result += getNickname()+":";
        }
        
        // If this is an VNC over SSH connection, add the SSH server:port in parentheses
        if (getConnectionType() == Constants.CONN_TYPE_SSH) {
            result += "(" + getSshServer() + ":" + getSshPort() + ")" + ":";
        }
        
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
    
    /**
     * Return the object representing the app global state in the database, or null
     * if the object hasn't been set up yet
     * @param db App's database -- only needs to be readable
     * @return Object representing the single persistent instance of MostRecentBean, which
     * is the app's global state
     */
    public static MostRecentBean getMostRecent(SQLiteDatabase db) {
        ArrayList<MostRecentBean> recents = new ArrayList<MostRecentBean>(1);
        MostRecentBean.getAll(db, MostRecentBean.GEN_TABLE_NAME, recents, MostRecentBean.GEN_NEW);
        if (recents.size() == 0)
            return null;
        return recents.get(0);
    }
    
    public void saveAndWriteRecent(boolean saveEmpty, Database database) {
        
        // We need server address or SSH server to be filled out to save. Otherwise,
        // we keep adding empty connections. 
        // However, if there is partial data from a URI, we can present the edit screen. 
        // Alternately, perhaps we could process some extra data
        if ((getConnectionType() == Constants.CONN_TYPE_SSH && getSshServer().equals("")
            || getAddress().equals("")) && !saveEmpty) {
            return;
        }
        
        SQLiteDatabase db = database.getWritableDatabase();
        db.beginTransaction();
        try {
            save(db);
            MostRecentBean mostRecent = getMostRecent(db);
            if (mostRecent == null) {
                mostRecent = new MostRecentBean();
                mostRecent.setConnectionId(get_Id());
                mostRecent.Gen_insert(db);
            } else {
                mostRecent.setConnectionId(get_Id());
                mostRecent.Gen_update(db);
            }
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
            db.close();
        }
        if (db.isOpen()) {
            db.close();
        }
    }
}
