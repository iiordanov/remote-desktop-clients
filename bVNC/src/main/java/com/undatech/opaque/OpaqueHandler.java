package com.undatech.opaque;

import java.security.NoSuchAlgorithmException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import android.util.Base64;
import android.widget.Toast;

import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.dialogs.GetTextFragment;
import com.iiordanov.bVNC.input.RemoteCanvasHandler;
import com.undatech.opaque.dialogs.ChoiceFragment;
import com.undatech.opaque.dialogs.MessageFragment;
import com.undatech.opaque.dialogs.SelectTextElementFragment;
import com.undatech.opaque.util.GooglePlayUtils;
import com.undatech.opaque.util.SslUtils;
import com.undatech.remoteClientUi.R;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;

public class OpaqueHandler extends Handler {
    private static String TAG = "OpaqueHandler";
    private Context context;
    private RemoteCanvas c;
    private Connection settings;
    private FragmentManager fm;
    
    public OpaqueHandler(Context context, RemoteCanvas c, Connection settings) {
        super();
        this.context = context;
        this.c = c;
        this.settings = settings;
        this.fm = ((FragmentActivity)context).getSupportFragmentManager();
    }

    private void showGetTextFragmentRemoteCanvas(String tag, String dialogId, String title,
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

    private void displayMessageAndFinish(int titleTextId, int messageTextId, int buttonTextId) {
        MessageFragment message = MessageFragment.newInstance(
                context.getString(titleTextId),
                context.getString(messageTextId),
                context.getString(buttonTextId),
                new MessageFragment.OnFragmentDismissedListener() {
                    @Override
                    public void onDialogDismissed() {
                        MessageDialogs.justFinish(context);
                    }
                });
        message.show(fm, "endingDialog");
        c.closeConnection();
    }
    
    @Override
    public void handleMessage(Message msg) {
        android.util.Log.d(TAG, "handleMessage: " + msg.what);
        if (msg.what == 0) {
            android.util.Log.d(TAG, "Ignoring message with ID 0");
            return;
        }
        final String messageText = Utils.getStringFromMessage(msg, "message");

        switch (msg.what) {
        case RemoteClientLibConstants.VV_OVER_HTTP_FAILURE:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_failed_to_download_vv_http,
                                                   R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.VV_OVER_HTTPS_FAILURE:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_failed_to_download_vv_https,
                                                   R.string.error_dialog_title);
        case RemoteClientLibConstants.VV_DOWNLOAD_TIMEOUT:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_vv_download_timeout,
                                                   R.string.error_dialog_title);
        case RemoteClientLibConstants.PVE_FAILED_TO_PARSE_JSON:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_failed_to_parse_json,
                                                   R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.PVE_VMID_NOT_NUMERIC:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_vmid_not_numeric,
                                                   R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.PVE_API_UNEXPECTED_CODE:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_api_unexpected_code,
                                                   R.string.error_dialog_title, msg.getData().getString("error"));
            break;
        case RemoteClientLibConstants.PVE_API_IO_ERROR:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_api_io_error,
                                                   R.string.error_dialog_title, msg.getData().getString("error"));
            break;
        case RemoteClientLibConstants.PVE_TIMEOUT_COMMUNICATING:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_timeout_communicating,
                                                   R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.PVE_NULL_DATA:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_null_data,
                                                   R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.VV_FILE_ERROR:
            c.disconnectAndShowMessage(R.string.vv_file_error, R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.NO_VM_FOUND_FOR_USER:
            c.disconnectAndShowMessage(R.string.error_no_vm_found_for_user, R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.OVIRT_SSL_HANDSHAKE_FAILURE:
            c.disconnectAndShowMessage(R.string.error_ovirt_ssl_handshake_failure, R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.VM_LOOKUP_FAILED:
            c.disconnectAndShowMessage(R.string.error_vm_lookup_failed, R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.VM_LAUNCHED:
            c.disconnectAndShowMessage(R.string.info_vm_launched_on_stand_by, R.string.info_dialog_title);
            break;
        case RemoteClientLibConstants.LAUNCH_VNC_VIEWER:
            android.util.Log.d(TAG, "Trying to launch VNC viewer");

            if (!GooglePlayUtils.isPackageInstalled(context, "com.iiordanov.bVNC") &&
                !GooglePlayUtils.isPackageInstalled(context, "com.iiordanov.freebVNC")) {
                displayMessageAndFinish(R.string.info_dialog_title, R.string.message_please_install_bvnc, R.string.ok);
                return;
            }

            String address = msg.getData().getString("address");
            String port = msg.getData().getString("port");
            String password = msg.getData().getString("password");
            String uriString = "vnc://" + address + ":" + port + "?VncPassword=" + password;
            Intent intent = new Intent(Intent.ACTION_VIEW).setType("application/vnd.vnc").setData(Uri.parse(uriString));
            context.startActivity(intent);
            MessageDialogs.justFinish(context);
            break;
        case RemoteClientLibConstants.GET_PASSWORD:
            showGetTextFragmentRemoteCanvas(context.getString(R.string.enter_password),
                    GetTextFragment.DIALOG_ID_GET_OPAQUE_PASSWORD,
                    context.getString(R.string.enter_password),
                    c, GetTextFragment.Password,
                    R.string.enter_password, R.string.enter_password,
                    settings.getPassword(), null, null,
                    settings.getKeepPassword());
            break;
        case RemoteClientLibConstants.GET_OTP_CODE:
            c.pd.dismiss();
            showGetTextFragmentRemoteCanvas(context.getString(R.string.enter_otp_code),
                    GetTextFragment.DIALOG_ID_GET_OPAQUE_OTP_CODE,
                    context.getString(R.string.enter_otp_code),
                    c, GetTextFragment.Plaintext,
                    R.string.enter_otp_code, R.string.enter_otp_code,
                    null, null, null,
                    false);
            break;
        case RemoteClientLibConstants.PVE_FAILED_TO_AUTHENTICATE:
            if (c.retrievevvFileName() != null) {
                MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_failed_to_authenticate,
                        R.string.error_dialog_title);
                break;
            }
            else {
                c.maintainConnection = false;
                showGetTextFragmentRemoteCanvas(context.getString(R.string.enter_password_auth_failed),
                        GetTextFragment.DIALOG_ID_GET_OPAQUE_CREDENTIALS,
                        context.getString(R.string.enter_password_auth_failed),
                        c, GetTextFragment.Credentials,
                        R.string.enter_password_auth_failed, R.string.enter_password_auth_failed,
                        settings.getUserName(), settings.getPassword(), null,
                        settings.getKeepPassword());
                break;
            }
        case RemoteClientLibConstants.OVIRT_AUTH_FAILURE:
            if (c.retrievevvFileName() != null) {
                c.disconnectAndShowMessage(R.string.error_ovirt_auth_failure, R.string.error_dialog_title);
                break;
            }
            else {
                c.maintainConnection = false;
                showGetTextFragmentRemoteCanvas(context.getString(R.string.enter_password_auth_failed),
                        GetTextFragment.DIALOG_ID_GET_OPAQUE_CREDENTIALS,
                        context.getString(R.string.enter_password_auth_failed),
                        c, GetTextFragment.Credentials,
                        R.string.enter_password_auth_failed, R.string.enter_password_auth_failed,
                        settings.getUserName(), settings.getPassword(), null,
                        settings.getKeepPassword());
                break;
            }
        case RemoteClientLibConstants.DIALOG_DISPLAY_VMS:
            c.pd.dismiss();
            ArrayList<String> vms = msg.getData().getStringArrayList("vms");

            if (vms.size() > 0) {
                fm = ((FragmentActivity)context).getSupportFragmentManager();
                SelectTextElementFragment displayVms = SelectTextElementFragment.newInstance(
                                                        context.getString(R.string.select_vm_title),
                                                        vms, (RemoteCanvasActivity)context);
                displayVms.setCancelable(false);
                displayVms.show(fm, "selectVm");
            }
            break;
        case RemoteClientLibConstants.SPICE_CONNECT_SUCCESS:
            c.pd.dismiss();
            synchronized(c.spicecomm) {
                c.spiceUpdateReceived = true;
                c.spicecomm.notifyAll();
            }
            break;
        case RemoteClientLibConstants.SPICE_CONNECT_FAILURE:
            String e = msg.getData().getString("message");
            c.pd.dismiss();
            // Only if we were intending to stay connected, and the connection failed, show an error message.
            if (c.maintainConnection) {
                if (!c.spiceUpdateReceived) {
                    c.disconnectAndShowMessage(R.string.error_ovirt_unable_to_connect, R.string.error_dialog_title, e);
                } else {
                    c.disconnectAndShowMessage(R.string.error_connection_interrupted, R.string.error_dialog_title, e);
                }
            }
            break;
        case RemoteClientLibConstants.OVIRT_TIMEOUT:
            c.pd.dismiss();
            c.disconnectAndShowMessage(R.string.error_ovirt_timeout, R.string.error_dialog_title);
            break;
        case RemoteClientLibConstants.DIALOG_X509_CERT:
            X509Certificate cert = (X509Certificate)msg.obj;
            validateX509Cert(cert);
            break;
        case RemoteClientLibConstants.DISCONNECT_NO_MESSAGE:
            c.closeConnection();
            MessageDialogs.justFinish(context);
            break;
        case RemoteClientLibConstants.SHOW_TOAST:
            this.post(new Runnable() {
                public void run() {
                    Toast.makeText(context, Utils.getStringResourceByName(context, messageText),
                                        Toast.LENGTH_LONG).show();
                }
            });
            break;
        case RemoteClientLibConstants.REINIT_SESSION:
            c.reinitializeOpaque();
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
                            settings.save(OpaqueHandler.this.context);
                            synchronized (OpaqueHandler.this) {
                                OpaqueHandler.this.notifyAll();
                            }
                        } else {
                            android.util.Log.e(TAG, "We were told not to continue");
                            MessageDialogs.justFinish(context);
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
