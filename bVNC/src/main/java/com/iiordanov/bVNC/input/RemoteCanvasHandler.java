package com.iiordanov.bVNC.input;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import android.widget.Toast;

import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.dialogs.GetTextFragment;
import com.undatech.opaque.Connection;
import com.undatech.opaque.MessageDialogs;
import com.undatech.opaque.RemoteClientLibConstants;
import java.security.cert.X509Certificate;

import com.undatech.remoteClientUi.R;

public class RemoteCanvasHandler extends Handler {
    private static String TAG = "RemoteCanvasHandler";
    private Context context;
    private RemoteCanvas c;
    private Connection settings;
    private FragmentManager fm;

    public RemoteCanvasHandler(Context context, RemoteCanvas c, Connection settings) {
        super();
        this.context = context;
        this.c = c;
        this.settings = settings;
        Activity activity = Utils.getActivity(context);
        if (activity instanceof FragmentActivity) {
            this.fm = ((FragmentActivity) Utils.getActivity(context)).getSupportFragmentManager();
        }
    }

    public RemoteCanvasHandler(Context context) {
        super();
        context = context;
    }

    public Connection getConnection() {
        return settings;
    }

    public void setConnection(Connection connection) {
        this.settings = connection;
    }

    private void showGetTextFragment(String tag, String dialogId, String title,
                                      GetTextFragment.OnFragmentDismissedListener dismissalListener,
                                      int dialogType, int messageNum, int errorNum,
                                      String t1, String t2, String t3, boolean keep) {
        if (c.pd != null && c.pd.isShowing()) {
            c.pd.dismiss();
        }
        GetTextFragment frag = GetTextFragment.newInstance(dialogId, title, dismissalListener,
                dialogType, messageNum, errorNum, t1, t2, t3, keep);
        frag.setCancelable(false);
        frag.show(fm, tag);
    }

    @Override
    public void handleMessage(final Message msg) {
        Bundle s;
        android.util.Log.d(TAG, "Handling message, msg.what: " + msg.what);
        final String messageText = Utils.getStringFromMessage(msg, "message");
        switch (msg.what) {
            case RemoteClientLibConstants.PRO_FEATURE:
                if (c.pd != null && c.pd.isShowing()) {
                    c.pd.dismiss();
                }
                c.showFatalMessageAndQuit(context.getString(R.string.pro_feature_mfa));
                break;
            case RemoteClientLibConstants.GET_VERIFICATIONCODE:
                showGetTextFragment(context.getString(R.string.verification_code),
                        GetTextFragment.DIALOG_ID_GET_VERIFICATIONCODE,
                        context.getString(R.string.verification_code),
                        c.sshConnection, GetTextFragment.Plaintext,
                        R.string.verification_code_message, R.string.verification_code,
                        null, null, null, false);
                break;
            case RemoteClientLibConstants.GET_SSH_CREDENTIALS:
                showGetTextFragment(context.getString(R.string.enter_ssh_credentials),
                        GetTextFragment.DIALOG_ID_GET_SSH_CREDENTIALS,
                        context.getString(R.string.enter_ssh_credentials),
                        c.sshConnection, GetTextFragment.CredentialsWithReadOnlyUser,
                        R.string.enter_ssh_credentials, R.string.enter_ssh_credentials,
                        settings.getSshUser(), settings.getSshPassword(), null,
                        settings.getKeepSshPassword());
                break;
            case RemoteClientLibConstants.GET_SSH_PASSPHRASE:
                showGetTextFragment(context.getString(R.string.ssh_passphrase_hint),
                        GetTextFragment.DIALOG_ID_GET_SSH_PASSPHRASE,
                        context.getString(R.string.enter_passphrase_title),
                        c.sshConnection, GetTextFragment.Password,
                        R.string.enter_passphrase, R.string.ssh_passphrase_hint,
                        null, null, null, settings.getKeepSshPassword());
                break;
            case RemoteClientLibConstants.GET_VNC_PASSWORD:
                showGetTextFragment(context.getString(R.string.enter_vnc_password),
                        GetTextFragment.DIALOG_ID_GET_VNC_PASSWORD,
                        context.getString(R.string.enter_vnc_password),
                        c, GetTextFragment.Password,
                        R.string.enter_vnc_password, R.string.enter_vnc_password,
                        settings.getPassword(), null, null, settings.getKeepPassword());
                break;
            case RemoteClientLibConstants.GET_VNC_CREDENTIALS:
                showGetTextFragment(context.getString(R.string.enter_vnc_credentials),
                        GetTextFragment.DIALOG_ID_GET_VNC_CREDENTIALS,
                        context.getString(R.string.enter_vnc_credentials),
                        c, GetTextFragment.Credentials,
                        R.string.enter_vnc_credentials, R.string.enter_vnc_credentials,
                        settings.getUserName(), settings.getPassword(), null,
                        settings.getKeepPassword());
                break;
            case RemoteClientLibConstants.GET_RDP_CREDENTIALS:
                showGetTextFragment(context.getString(R.string.enter_rdp_credentials),
                        GetTextFragment.DIALOG_ID_GET_RDP_CREDENTIALS,
                        context.getString(R.string.enter_rdp_credentials),
                        c, GetTextFragment.CredentialsWithDomain,
                        R.string.enter_rdp_credentials, R.string.enter_rdp_credentials,
                        settings.getUserName(), settings.getRdpDomain(), settings.getPassword(),
                        settings.getKeepPassword());
                break;
            case RemoteClientLibConstants.GET_SPICE_PASSWORD:
                showGetTextFragment(context.getString(R.string.enter_spice_password),
                        GetTextFragment.DIALOG_ID_GET_SPICE_PASSWORD,
                        context.getString(R.string.enter_spice_password),
                        c, GetTextFragment.Password,
                        R.string.enter_spice_password, R.string.enter_spice_password,
                        settings.getPassword(), null, null, settings.getKeepPassword());
                break;
            case RemoteClientLibConstants.DIALOG_X509_CERT:
                android.util.Log.d(TAG, "DIALOG_X509_CERT");
                c.validateX509Cert((X509Certificate) msg.obj);
                break;
            case RemoteClientLibConstants.DIALOG_SSH_CERT:
                android.util.Log.d(TAG, "DIALOG_SSH_CERT");
                c.initializeSshHostKey();
                break;
            case RemoteClientLibConstants.DIALOG_RDP_CERT:
                android.util.Log.d(TAG, "DIALOG_RDP_CERT");
                s = (Bundle) msg.obj;
                c.validateRdpCert(s.getString("subject"), s.getString("issuer"), s.getString("fingerprint"));
                break;
            case RemoteClientLibConstants.DISCONNECT_WITH_MESSAGE:
                c.showFatalMessageAndQuit(messageText);
                break;
            case RemoteClientLibConstants.SHOW_TOAST:
                this.post(new Runnable() {
                    public void run() {
                        Toast.makeText(context, Utils.getStringResourceByName(context, messageText),
                                        Toast.LENGTH_LONG).show();
                    }
                });
                break;
            case RemoteClientLibConstants.SPICE_CONNECT_FAILURE_IF_MAINTAINING_CONNECTION:
                if (c.maintainConnection) {
                    c.showFatalMessageAndQuit(messageText);
                }
                break;
            case RemoteClientLibConstants.DISCONNECT_NO_MESSAGE:
                c.closeConnection();
                MessageDialogs.justFinish(context);
                break;
            case RemoteClientLibConstants.SPICE_CONNECT_SUCCESS:
                if (c.pd != null && c.pd.isShowing()) {
                    c.pd.dismiss();
                }
                break;
            case RemoteClientLibConstants.SPICE_CONNECT_FAILURE:
                if (c.maintainConnection) {
                    c.maintainConnection = false;
                    if (c.pd != null && c.pd.isShowing()) {
                        c.pd.dismiss();
                    }
                    if (!c.spiceUpdateReceived) {
                        c.showFatalMessageAndQuit(context.getString(R.string.error_spice_unable_to_connect));
                    } else {
                        c.showFatalMessageAndQuit(context.getString(R.string.error_connection_interrupted));
                    }
                }
                break;
            case RemoteClientLibConstants.SPICE_TLS_ERROR:
                c.showFatalMessageAndQuit(context.getString(R.string.error_ovirt_ssl_handshake_failure));
                break;
            case RemoteClientLibConstants.RDP_CONNECT_FAILURE:
                if (c.maintainConnection) {
                    c.showFatalMessageAndQuit(context.getString(R.string.error_rdp_connection_failed));
                }
                break;
            case RemoteClientLibConstants.RDP_UNABLE_TO_CONNECT:
                if (c.maintainConnection) {
                    c.showFatalMessageAndQuit(context.getString(R.string.error_rdp_unable_to_connect));
                }
                break;
            case RemoteClientLibConstants.RDP_AUTH_FAILED:
                if (c.maintainConnection) {
                    c.showFatalMessageAndQuit(context.getString(R.string.error_rdp_authentication_failed));
                }
                break;
            case RemoteClientLibConstants.SERVER_CUT_TEXT:
                s = (Bundle) msg.obj;
                c.serverJustCutText = true;
                c.setClipboardText(s.getString("text"));
                break;
            case RemoteClientLibConstants.REINIT_SESSION:
                c.reinitializeCanvas();
                break;
            case RemoteClientLibConstants.REPORT_TOOLBAR_POSITION:
                android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION");
                if (settings.getUseLastPositionToolbar()) {
                    int useLastPositionToolbarX = Utils.getIntFromMessage(msg, "useLastPositionToolbarX");
                    int useLastPositionToolbarY = Utils.getIntFromMessage(msg, "useLastPositionToolbarY");
                    boolean useLastPositionToolbarMoved = Utils.getBooleanFromMessage(msg, "useLastPositionToolbarMoved");
                    android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION, X Coordinate" + useLastPositionToolbarX);
                    android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION, Y Coordinate" + useLastPositionToolbarY);
                    settings.setUseLastPositionToolbarX(useLastPositionToolbarX);
                    settings.setUseLastPositionToolbarY(useLastPositionToolbarY);
                    settings.setUseLastPositionToolbarMoved(useLastPositionToolbarMoved);
                }
                break;
            default:
                android.util.Log.e(TAG, "Not handling unknown messageId: " + msg.what);
                break;
        }
    }


}
