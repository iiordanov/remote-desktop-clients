package com.iiordanov.bVNC.input;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.util.Base64;

import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.undatech.opaque.Connection;
import com.undatech.opaque.MessageDialogs;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.dialogs.ChoiceFragment;
import com.undatech.opaque.dialogs.GetTextFragment;
import com.undatech.opaque.dialogs.MessageFragment;

import java.security.NoSuchAlgorithmException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;

import com.undatech.opaque.util.SslUtils;
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
        this.fm = ((FragmentActivity)context).getSupportFragmentManager();
    }
    
    private void showGetTextFragment (String id, String title, boolean password) {
        GetTextFragment frag = GetTextFragment.newInstance(id, title, (RemoteCanvasActivity)context, password);
        frag.show(fm, id);
    }
    
    private void displayMessageAndFinish(int titleTextId, int messageTextId, int buttonTextId) {
        MessageFragment message = MessageFragment.newInstance(
                context.getString(titleTextId),
                context.getString(messageTextId),
                context.getString(buttonTextId),
                new MessageFragment.OnFragmentDismissedListener() {
                    @Override
                    public void onDialogDismissed() {
                        ((Activity)context).finish();
                    }
                });
        message.show(fm, "endingDialog");
        c.disconnectAndCleanUp();
    }

    @Override
    public void handleMessage(Message msg) {
        FragmentManager fm = null;
        Bundle s = null;

        android.util.Log.d(TAG, "Handling message, msg.what: " + msg.what);

        switch (msg.what) {
            case Constants.PRO_FEATURE:
                if (c.pd != null && c.pd.isShowing()) {
                    c.pd.dismiss();
                }
                c.showFatalMessageAndQuit(context.getString(R.string.pro_feature_mfa));
                break;
            case Constants.GET_VERIFICATIONCODE:
                if (c.pd != null && c.pd.isShowing()) {
                    c.pd.dismiss();
                }
                fm = ((FragmentActivity) context).getSupportFragmentManager();
                com.iiordanov.bVNC.dialogs.GetTextFragment getVerificationCode =
                        com.iiordanov.bVNC.dialogs.GetTextFragment.newInstance(
                                com.iiordanov.bVNC.dialogs.GetTextFragment.DIALOG_ID_GET_VERIFICATIONCODE,
                                context.getString(R.string.verification_code),
                                c.sshConnection, com.iiordanov.bVNC.dialogs.GetTextFragment.Plaintext,
                                R.string.verification_code_message, R.string.verification_code);
                getVerificationCode.setCancelable(false);
                getVerificationCode.show(fm, context.getString(R.string.verification_code));
                break;
            case Constants.GET_SSH_PASSWORD:
                if (c.pd != null && c.pd.isShowing()) {
                    c.pd.dismiss();
                }
                fm = ((FragmentActivity) context).getSupportFragmentManager();
                com.iiordanov.bVNC.dialogs.GetTextFragment getSshPassword =
                        com.iiordanov.bVNC.dialogs.GetTextFragment.newInstance(
                                com.iiordanov.bVNC.dialogs.GetTextFragment.DIALOG_ID_GET_SSH_PASSWORD,
                                context.getString(R.string.enter_password),
                                c.sshConnection, com.iiordanov.bVNC.dialogs.GetTextFragment.Password,
                                R.string.enter_ssh_password_message, R.string.password_hint_ssh);
                getSshPassword.setCancelable(false);
                getSshPassword.show(fm, context.getString(R.string.verification_code));
                break;
            case Constants.GET_SSH_PASSPHRASE:
                if (c.pd != null && c.pd.isShowing()) {
                    c.pd.dismiss();
                }
                fm = ((FragmentActivity) context).getSupportFragmentManager();
                com.iiordanov.bVNC.dialogs.GetTextFragment getSshPassPhrase =
                        com.iiordanov.bVNC.dialogs.GetTextFragment.newInstance(
                                com.iiordanov.bVNC.dialogs.GetTextFragment.DIALOG_ID_GET_SSH_PASSPHRASE,
                                context.getString(R.string.enter_passphrase),
                                c.sshConnection, com.iiordanov.bVNC.dialogs.GetTextFragment.Password,
                                R.string.enter_ssh_passphrase_message, R.string.ssh_passphrase_hint);
                getSshPassPhrase.setCancelable(false);
                getSshPassPhrase.show(fm, context.getString(R.string.verification_code));
                break;
            case Constants.DIALOG_X509_CERT:
                android.util.Log.d(TAG, "DIALOG_X509_CERT");
                validateX509Cert((X509Certificate) msg.obj);
                break;
            case Constants.DIALOG_SSH_CERT:
                android.util.Log.d(TAG, "DIALOG_SSH_CERT");
                c.initializeSshHostKey();
                break;
            case Constants.DIALOG_RDP_CERT:
                android.util.Log.d(TAG, "DIALOG_RDP_CERT");
                s = (Bundle) msg.obj;
                c.validateRdpCert(s.getString("subject"), s.getString("issuer"), s.getString("fingerprint"));
                break;
            case Constants.SPICE_CONNECT_SUCCESS:
                if (c.pd != null && c.pd.isShowing()) {
                    c.pd.dismiss();
                }
                break;
            case Constants.SPICE_CONNECT_FAILURE:
                if (c.maintainConnection) {
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
            case Constants.RDP_CONNECT_FAILURE:
                if (c.maintainConnection) {
                    c.showFatalMessageAndQuit(context.getString(R.string.error_rdp_connection_failed));
                }
                break;
            case Constants.RDP_UNABLE_TO_CONNECT:
                if (c.maintainConnection) {
                    c.showFatalMessageAndQuit(context.getString(R.string.error_rdp_unable_to_connect));
                }
                break;
            case Constants.RDP_AUTH_FAILED:
                if (c.maintainConnection) {
                    c.showFatalMessageAndQuit(context.getString(R.string.error_rdp_authentication_failed));
                }
                break;
            case Constants.SERVER_CUT_TEXT:
                s = (Bundle) msg.obj;
                c.serverJustCutText = true;
                c.setClipboardText(s.getString("text"));
                break;
        }
    }
    /**
     * If there is a saved cert, checks the one given against it. If a signature was passed in
     * and no saved cert, then check that signature. Otherwise, presents the
     * given cert's signature to the user for approval.
     * 
     * The saved data must always win over any passed-in URI data
     * 
     * @param cert the given cert.
     */
    private void validateX509Cert (final X509Certificate cert) {
        
        byte[] certDataTemp = null;
        try {
            certDataTemp = cert.getEncoded();
        } catch (Exception ex) {
            ex.printStackTrace();
            return;
        }   
        final byte[] certData = certDataTemp;
        
        String md5Hash = "";
        String sha1Hash = "";
        String sha256Hash = "";
        try {
            md5Hash = SslUtils.signature("MD5", certData);
            sha1Hash = SslUtils.signature("SHA-1", certData);
            sha256Hash = SslUtils.signature("SHA-256", certData);
        } catch (NoSuchAlgorithmException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }

        String certInfo;
            certInfo = String.format(context.getString(R.string.info_cert), md5Hash, sha1Hash, sha256Hash,
            cert.getSubjectX500Principal().getName(),
            cert.getIssuerX500Principal().getName(),
            cert.getNotBefore(),
            cert.getNotAfter()
            );

        // TODO: Create title string
        ChoiceFragment certDialog = ChoiceFragment.newInstance(
                context.getString(R.string.ca_new_or_changed),
                certInfo,
                "Accept",
                "Reject",
                new ChoiceFragment.OnFragmentDismissedListener() {

                    @Override
                    public void onResponseObtained(boolean result) {
                        if (result) {
                            android.util.Log.e(TAG, "We were told to continue");
                            settings.setOvirtCaData(Base64.encodeToString(certData, Base64.DEFAULT));
                            settings.save(RemoteCanvasHandler.this.context);
                            synchronized (RemoteCanvasHandler.this) {
                                RemoteCanvasHandler.this.notify();
                            }
                        } else {
                            android.util.Log.e(TAG, "We were told not to continue");
                            ((Activity)context).finish();
                        }
                    }
                });
        
        FragmentManager fm = ((FragmentActivity)context).getSupportFragmentManager();
        certDialog.setCancelable(false);
        certDialog.show(fm, "certDialog");

    }
    
    /**
     * Convenience function to create a message with a single key and String value pair in it.
     * @param what
     * @param key
     * @param value
     * @return
     */
    public static Message getMessageString (int what, String key, String value) {
        Message m = new Message();
        m.what = what;
        Bundle d = new Bundle();
        d.putString(key, value);
        m.setData(d);
        return m;
    }
    
    /**
     * Convenience function to create a message with a single key and String value pair in it.
     * @param what
     * @param key
     * @param value
     * @return
     */
    public static Message getMessageStringList (int what, String key, ArrayList<String> value) {
        Message m = new Message();
        m.what = what;
        Bundle d = new Bundle();
        d.putStringArrayList(key, value);
        m.setData(d);
        return m;
    }
}
