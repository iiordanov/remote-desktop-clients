package com.undatech.opaque;

import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.text.ChoiceFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.util.Base64;
import android.util.Log;

import com.undatech.opaque.R;
import com.undatech.opaque.dialogs.ChoiceFragment;
import com.undatech.opaque.dialogs.GetTextFragment;
import com.undatech.opaque.dialogs.SelectTextElementFragment;
import com.undatech.opaque.util.SslUtils;

public class RemoteCanvasActivityHandler extends Handler {
    private static String TAG = "RemoteCanvasActivityHandler";
    private Context context;
    private RemoteCanvas c;
    private ConnectionSettings settings;
    
    public RemoteCanvasActivityHandler(Context context, RemoteCanvas c, ConnectionSettings settings) {
        super();
        this.context = context;
        this.c = c;
        this.settings = settings;
    }
    
    @Override
    public void handleMessage(Message msg) {
        FragmentManager fm;
        switch (msg.what) {
        case Constants.VV_OVER_HTTP_FAILURE:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_failed_to_download_vv_http,
                                                   R.string.error_dialog_title);
            break;
        case Constants.VV_OVER_HTTPS_FAILURE:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_failed_to_download_vv_https,
                                                   R.string.error_dialog_title);
        case Constants.VV_DOWNLOAD_TIMEOUT:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_vv_download_timeout,
                                                   R.string.error_dialog_title);
        case Constants.PVE_FAILED_TO_AUTHENTICATE:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_failed_to_authenticate,
                                                   R.string.error_dialog_title);
            break;
        case Constants.PVE_FAILED_TO_PARSE_JSON:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_failed_to_parse_json,
                                                   R.string.error_dialog_title);
            break;
        case Constants.PVE_VMID_NOT_NUMERIC:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_vmid_not_numeric,
                                                   R.string.error_dialog_title);
            break;
        case Constants.PVE_API_UNEXPECTED_CODE:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_api_unexpected_code,
                                                   R.string.error_dialog_title, msg.getData().getString("error"));
            break;
        case Constants.PVE_API_IO_ERROR:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_api_io_error,
                                                   R.string.error_dialog_title, msg.getData().getString("error"));
            break;
        case Constants.PVE_TIMEOUT_COMMUNICATING:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_timeout_communicating,
                                                   R.string.error_dialog_title);
            break;
        case Constants.PVE_NULL_DATA:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_null_data,
                                                   R.string.error_dialog_title);
            break;
        case Constants.VV_FILE_ERROR:
            c.disconnectAndShowMessage(R.string.vv_file_error, R.string.error_dialog_title);
            break;
        case Constants.NO_VM_FOUND_FOR_USER:
            c.disconnectAndShowMessage(R.string.error_no_vm_found_for_user, R.string.error_dialog_title);
            break;
        case Constants.OVIRT_SSL_HANDSHAKE_FAILURE:
            c.disconnectAndShowMessage(R.string.error_ovirt_ssl_handshake_failure, R.string.error_dialog_title);
            break;
        case Constants.VM_LOOKUP_FAILED:
            c.disconnectAndShowMessage(R.string.error_vm_lookup_failed, R.string.error_dialog_title);
            break;
        case Constants.OVIRT_AUTH_FAILURE:
            c.disconnectAndShowMessage(R.string.error_ovirt_auth_failure, R.string.error_dialog_title);
            break;
        case Constants.VM_LAUNCHED:
            c.disconnectAndShowMessage(R.string.info_vm_launched_on_stand_by, R.string.info_dialog_title);
            break;
        case Constants.LAUNCH_VNC_VIEWER:
/* TODO: Implement:
            android.util.Log.d(TAG, "Trying to launch VNC viewer");
            Intent intent = new Intent ();
            String intentURI = "vnc://" + address + ":" + port + "?password=" + password;
            try {
                intent = Intent.parseUri(intentURI, Intent.URI_INTENT_SCHEME);
            } catch (URISyntaxException e) {
            }

            myself.canvas.getContext().startActivity(intent);
            disconnectAndCleanup();
            finish();
*/
            c.disconnectAndShowMessage(R.string.error_no_vnc_support_yet, R.string.error_dialog_title);
            break;
        case Constants.GET_PASSWORD:
            c.progressDialog.dismiss();

            fm = ((FragmentActivity)context).getSupportFragmentManager();
            GetTextFragment getPassword = GetTextFragment.newInstance(
                                                    context.getString(R.string.enter_password),
                                                    (RemoteCanvasActivity)context, true);
            // TODO: Add OK button.
            //newFragment.setCancelable(false);
            getPassword.show(fm, "getPassword");
            break;
        case Constants.DIALOG_DISPLAY_VMS:
            c.progressDialog.dismiss();
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
        case Constants.SPICE_CONNECT_SUCCESS:
            c.progressDialog.dismiss();
            synchronized(c.spicecomm) {
                c.spiceUpdateReceived = true;
                c.spicecomm.notifyAll();
            }
            break;
        case Constants.SPICE_CONNECT_FAILURE:
            c.progressDialog.dismiss();
            // Only if we were intending to stay connected, and the connection failed, show an error message.
            if (c.stayConnected) {
                if (!c.spiceUpdateReceived) {
                    c.disconnectAndShowMessage(R.string.error_ovirt_unable_to_connect, R.string.error_dialog_title);
                } else {
                    c.disconnectAndShowMessage(R.string.error_connection_interrupted, R.string.error_dialog_title);
                }
            }
            break;
        case Constants.OVIRT_TIMEOUT:
            c.progressDialog.dismiss();
            c.disconnectAndShowMessage(R.string.error_ovirt_timeout, R.string.error_dialog_title);
            break;
        case Constants.DIALOG_X509_CERT:
            X509Certificate cert = (X509Certificate)msg.obj;
            validateX509Cert(cert);
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
                                                            settings.saveToSharedPreferences(RemoteCanvasActivityHandler.this.context);
                                                            synchronized (RemoteCanvasActivityHandler.this) {
                                                                RemoteCanvasActivityHandler.this.notify();
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
