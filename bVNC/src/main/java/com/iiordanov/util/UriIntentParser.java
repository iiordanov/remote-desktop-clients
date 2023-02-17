package com.iiordanov.util;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;
import android.widget.ImageView;

import com.iiordanov.bVNC.AbstractConnectionBean;
import com.iiordanov.bVNC.COLORMODEL;
import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.Database;
import com.iiordanov.bVNC.MostRecentBean;
import com.iiordanov.bVNC.Utils;
import com.undatech.opaque.Connection;

import net.sqlcipher.database.SQLiteDatabase;

import org.jetbrains.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class UriIntentParser {
    private static final String TAG = "UriIntentParser";

    public static ConnectionBean loadFromUriOrCreateNew(Uri dataUri, Context ctx) {
        android.util.Log.d(TAG, "loadFromUriOrCreateNew");
        ConnectionBean connection = new ConnectionBean(ctx);
        if (dataUri == null) {
            return connection;
        }
        ConnectionBean connectionById = tryFindingByWidgetId(dataUri, ctx, connection);
        if (connectionById != null) return connectionById;

        ConnectionBean connectionByNickname = tryFindingByNickname(dataUri, ctx, connection);
        if (connectionByNickname != null) return connectionByNickname;

        ConnectionBean connectionByHostname = tryFindingByHostname(dataUri, ctx, connection);
        if (connectionByHostname != null) return connectionByHostname;

        return connection;
    }

    private static ConnectionBean tryFindingByHostname(Uri dataUri, Context ctx, ConnectionBean connection) {
        String host = dataUri.getHost();
        return tryFindingByField(ctx, connection, AbstractConnectionBean.GEN_FIELD_ADDRESS, host);
    }

    private static ConnectionBean tryFindingByNickname(Uri dataUri, Context ctx, ConnectionBean connection) {
        String connectionName = dataUri.getQueryParameter(Constants.PARAM_CONN_NAME);
        return tryFindingByField(ctx, connection, AbstractConnectionBean.GEN_FIELD_NICKNAME, connectionName);
    }

    private static ConnectionBean tryFindingByField(Context ctx, ConnectionBean connection, String field, String value) {
        Database database = new Database(ctx);
        SQLiteDatabase queryDb = database.getReadableDatabase();
        Cursor cursor = null;
        ConnectionBean connectionByField = null;
        if (field != null && value != null) {
            cursor = queryDb.query(
                    AbstractConnectionBean.GEN_TABLE_NAME,
                    null,
                    field + " = ?",
                    new String[]{value},
                    null,
                    null,
                    null);
        }
        if (cursor != null && cursor.moveToFirst()) {
            Log.i(TAG, String.format(Locale.US, "Loading connection info from field: %s, value: %s", field, value));
            connectionByField = new ConnectionBean(ctx);
            connectionByField.Gen_populate(cursor, connection.Gen_columnIndices(cursor));
            cursor.close();
        }
        database.close();
        return connectionByField;
    }

    @Nullable
    private static ConnectionBean tryFindingByWidgetId(Uri dataUri, Context ctx, ConnectionBean connection) {
        String host = dataUri.getHost();
        if (host != null && host.startsWith(Utils.getConnectionString(ctx))) {
            getConnectionForShortcutWidget(ctx, connection, host);
            return connection;
        }
        return null;
    }

    private static void getConnectionForShortcutWidget(
            Context context,
            ConnectionBean connection,
            String host
    ) {
        Database database = new Database(context);
        int connectionId = 0;
        int idx = host.indexOf(':');

        if (idx != -1) {
            try {
                connectionId = Integer.parseInt(host.substring(idx + 1));
            } catch (NumberFormatException nfe) {
            }
        }

        if (connection.Gen_read(database.getReadableDatabase(), connectionId)) {
            MostRecentBean bean = ConnectionBean.getMostRecent(database.getReadableDatabase());
            if (bean != null) {
                bean.setConnectionId(connection.get_Id());
                bean.Gen_update(database.getWritableDatabase());
            }
        }
        database.close();
    }

    public static void parseFromUri(Context context, Connection connection, Uri dataUri) {
        Log.i(TAG, "Parsing URI.");
        if (dataUri == null) {
            connection.setReadyForConnection(false);
            connection.setReadyToBeSaved(true);
            return;
        }

        String host = dataUri.getHost();
        if (host != null) {
            connection.setAddress(host);

            // by default, the connection name is the host name
            String nickName = connection.getNickname();
            if (Utils.isNullOrEmptry(nickName)) {
                connection.setNickname(host);
            }
        }

        final int PORT_NONE = -1;
        int port = dataUri.getPort();
        if (port != PORT_NONE && !isValidPort(port)) {
            throw new IllegalArgumentException("The specified VNC port is not valid.");
        }
        connection.setPort(port);

        // handle legacy android-vnc-viewer parsing vnc://host:port/colormodel/password
        List<String> path = dataUri.getPathSegments();
        if (path.size() >= 1) {
            connection.setColorModel(path.get(0));
        }

        if (path.size() >= 2) {
            connection.setPassword(path.get(1));
        }

        // query based parameters
        String connectionName = dataUri.getQueryParameter(Constants.PARAM_CONN_NAME);

        if (connectionName != null) {
            connection.setNickname(connectionName);
        }

        ArrayList<String> supportedUserParams = new ArrayList<String>() {{
            add(Constants.PARAM_RDP_USER);
            add(Constants.PARAM_SPICE_USER);
            add(Constants.PARAM_VNC_USER);
        }};
        for (String userParam : supportedUserParams) {
            String username = dataUri.getQueryParameter(userParam);
            if (username != null) {
                connection.setUserName(username);
                break;
            }
        }

        ArrayList<String> supportedPwdParams = new ArrayList<String>() {{
            add(Constants.PARAM_RDP_PWD);
            add(Constants.PARAM_SPICE_PWD);
            add(Constants.PARAM_VNC_PWD);
        }};
        for (String pwdParam : supportedPwdParams) {
            String password = dataUri.getQueryParameter(pwdParam);
            if (password != null) {
                connection.setPassword(password);
                break;
            }
        }

        String securityTypeParam = dataUri.getQueryParameter(Constants.PARAM_SECTYPE);
        int secType = 0; //invalid
        if (securityTypeParam != null) {
            secType = Integer.parseInt(securityTypeParam); // throw if invalid
            switch (secType) {
                case Constants.SECTYPE_NONE:
                case Constants.SECTYPE_VNC:
                    connection.setConnectionType(Constants.CONN_TYPE_PLAIN);
                    break;
                case Constants.SECTYPE_INTEGRATED_SSH:
                    connection.setConnectionType(Constants.CONN_TYPE_SSH);
                    break;
                case Constants.SECTYPE_ULTRA:
                    connection.setConnectionType(Constants.CONN_TYPE_ULTRAVNC);
                    break;
                case Constants.SECTYPE_TLS:
                    connection.setConnectionType(Constants.CONN_TYPE_ANONTLS);
                    break;
                case Constants.SECTYPE_VENCRYPT:
                    connection.setConnectionType(Constants.CONN_TYPE_VENCRYPT);
                    break;
                case Constants.SECTYPE_TUNNEL:
                    connection.setConnectionType(Constants.CONN_TYPE_STUNNEL);
                    break;
                default:
                    throw new IllegalArgumentException("The specified security type is invalid or unsupported.");
            }
        }

        // ssh parameters
        String sshHost = dataUri.getQueryParameter(Constants.PARAM_SSH_HOST);
        if (sshHost != null) {
            connection.setSshServer(sshHost);
        }

        String sshPortParam = dataUri.getQueryParameter(Constants.PARAM_SSH_PORT);
        if (sshPortParam != null) {
            int sshPort = Integer.parseInt(sshPortParam);
            if (!isValidPort(sshPort))
                throw new IllegalArgumentException("The specified SSH port is not valid.");
            connection.setSshPort(sshPort);
        }

        String sshUser = dataUri.getQueryParameter(Constants.PARAM_SSH_USER);
        if (sshUser != null) {
            connection.setSshUser(sshUser);
        }

        String sshPassword = dataUri.getQueryParameter(Constants.PARAM_SSH_PWD);
        if (sshPassword != null) {
            connection.setSshPassword(sshPassword);
        }

        // security hashes
        String idHashAlgParam = dataUri.getQueryParameter(Constants.PARAM_ID_HASH_ALG);
        if (idHashAlgParam != null) {
            int idHashAlg = Integer.parseInt(idHashAlgParam); // throw if invalid
            switch (idHashAlg) {
                case Constants.ID_HASH_MD5:
                case Constants.ID_HASH_SHA1:
                case Constants.ID_HASH_SHA256:
                    connection.setIdHashAlgorithm(idHashAlg);
                    break;
                default:
                    // we are given a bad parameter
                    throw new IllegalArgumentException("The specified hash algorithm is invalid or unsupported.");
            }
        }

        String idHash = dataUri.getQueryParameter(Constants.PARAM_ID_HASH);
        if (idHash != null) {
            connection.setIdHash(idHash);
        }

        String viewOnlyParam = dataUri.getQueryParameter(Constants.PARAM_VIEW_ONLY);
        if (viewOnlyParam != null) connection.setViewOnly(Boolean.parseBoolean(viewOnlyParam));

        String scaleModeParam = dataUri.getQueryParameter(Constants.PARAM_SCALE_MODE);
        if (scaleModeParam != null)
            connection.setScaleMode(ImageView.ScaleType.valueOf(scaleModeParam));

        String extraKeysToggleParam = dataUri.getQueryParameter(Constants.PARAM_EXTRAKEYS_TOGGLE);
        if (extraKeysToggleParam != null)
            connection.setExtraKeysToggleType(Integer.parseInt(extraKeysToggleParam));

        // color model
        String colorModelParam = dataUri.getQueryParameter(Constants.PARAM_COLORMODEL);
        if (colorModelParam != null) {
            int colorModel = Integer.parseInt(colorModelParam); // throw if invalid
            switch (colorModel) {
                case Constants.COLORMODEL_BLACK_AND_WHITE:
                    connection.setColorModel(COLORMODEL.C2.nameString());
                    break;
                case Constants.COLORMODEL_GREYSCALE:
                    connection.setColorModel(COLORMODEL.C4.nameString());
                    break;
                case Constants.COLORMODEL_8_COLORS:
                    connection.setColorModel(COLORMODEL.C8.nameString());
                    break;
                case Constants.COLORMODEL_64_COLORS:
                    connection.setColorModel(COLORMODEL.C64.nameString());
                    break;
                case Constants.COLORMODEL_256_COLORS:
                    connection.setColorModel(COLORMODEL.C256.nameString());
                    break;
                // use the best currently available model
                case Constants.COLORMODEL_16BIT:
                    connection.setColorModel(COLORMODEL.C24bit.nameString());
                    break;
                case Constants.COLORMODEL_24BIT:
                    connection.setColorModel(COLORMODEL.C24bit.nameString());
                    break;
                case Constants.COLORMODEL_32BIT:
                    connection.setColorModel(COLORMODEL.C24bit.nameString());
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
            connection.setTlsPort(tlsPort);
        }

        // Parse a CA Cert path parameter
        String caCertPath = dataUri.getQueryParameter(Constants.PARAM_CACERT_PATH);
        if (caCertPath != null) {
            connection.setCaCertPath(caCertPath);
        }

        // Parse a Cert subject
        String certSubject = dataUri.getQueryParameter(Constants.PARAM_CERT_SUBJECT);
        if (certSubject != null) {
            connection.setCertSubject(certSubject);
        }

        // Parse a keyboard layout parameter
        String keyboardLayout = dataUri.getQueryParameter(Constants.PARAM_KEYBOARD_LAYOUT);
        if (keyboardLayout != null) {
            connection.setLayoutMap(keyboardLayout);
        }

        connection.determineIfReadyForConnection(secType);

        if (saveConnection && connection.isReadyToBeSaved()) {
            connection.saveAndWriteRecent(false, context);
        }
    }

    static boolean isValidPort(int port) {
        final int PORT_MAX = 65535;
        if (port <= 0 || port > PORT_MAX)
            return false;
        return true;
    }
}
