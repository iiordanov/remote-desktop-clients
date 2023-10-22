package com.iiordanov.bVNC.input;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Base64;
import android.util.Log;
import android.widget.Toast;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.iiordanov.bVNC.SSHConnection;
import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.dialogs.GetTextFragment;
import com.undatech.opaque.Connection;
import com.undatech.opaque.MessageDialogs;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.dialogs.ChoiceFragment;
import com.undatech.opaque.dialogs.MessageFragment;
import com.undatech.opaque.dialogs.SelectTextElementFragment;
import com.undatech.opaque.util.GooglePlayUtils;
import com.undatech.opaque.util.SslUtils;
import com.undatech.remoteClientUi.R;

import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;

public class RemoteCanvasHandler extends Handler {
    private static String TAG = "RemoteCanvasHandler";
    private Context context;
    private RemoteCanvas c;
    private Connection connection;
    private FragmentManager fm;

    private SSHConnection sshConnection;

    public RemoteCanvasHandler(Context context,
                               RemoteCanvas c,
                               Connection connection) {
        super();
        this.context = context;
        this.c = c;
        this.connection = connection;
        Activity activity = Utils.getActivity(context);
        if (activity instanceof FragmentActivity) {
            this.fm = ((FragmentActivity) Utils.getActivity(context)).getSupportFragmentManager();
        }
    }

    public RemoteCanvasHandler(Context context) {
        super();
        context = context;
    }

    /**
     * Convenience function to create a message with a single key and String value pair in it.
     *
     * @param what
     * @param key
     * @param value
     * @return
     */
    public static Message getMessageString(int what, String key, String value) {
        Message m = new Message();
        m.what = what;
        Bundle d = new Bundle();
        d.putString(key, value);
        m.setData(d);
        return m;
    }

    /**
     * Convenience function to create a message with a single key and String value pair in it.
     *
     * @param what
     * @param key
     * @param value
     * @return
     */
    public static Message getMessageStringList(int what, String key, ArrayList<String> value) {
        Message m = new Message();
        m.what = what;
        Bundle d = new Bundle();
        d.putStringArrayList(key, value);
        m.setData(d);
        return m;
    }

    public Connection getConnection() {
        return connection;
    }

    public void setConnection(Connection connection) {
        this.connection = connection;
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
                        Utils.justFinish(context);
                    }
                });
        message.show(fm, "endingDialog");
        c.closeConnection();
    }


    /**
     * If there is a saved cert, checks the one given against it. If a signature was passed in
     * and no saved cert, then check that signature. Otherwise, presents the
     * given cert's signature to the user for approval.
     * <p>
     * The saved data must always win over any passed-in URI data
     *
     * @param cert the given cert.
     */
    @SuppressLint("StringFormatInvalid")
    private void validateX509Cert(final X509Certificate cert) {
        byte[] certDataTemp;
        try {
            certDataTemp = cert.getEncoded();
        } catch (Exception ex) {
            ex.printStackTrace();
            return;
        }

        String md5Hash = "";
        String sha1Hash = "";
        String sha256Hash = "";
        try {
            md5Hash = SslUtils.signature("MD5", certDataTemp);
            sha1Hash = SslUtils.signature("SHA-1", certDataTemp);
            sha256Hash = SslUtils.signature("SHA-256", certDataTemp);
        } catch (NoSuchAlgorithmException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }

        String certInfo;
        certInfo = String.format(
                context.getString(R.string.info_cert),
                md5Hash, sha1Hash, sha256Hash,
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
                            saveAndAcceptCert(cert);
                        } else {
                            android.util.Log.e(TAG, "We were told not to continue");
                            Utils.justFinish(context);
                        }
                    }
                });

        FragmentManager fm = ((FragmentActivity) context).getSupportFragmentManager();
        certDialog.setCancelable(false);
        certDialog.show(fm, "certDialog");
    }


    /**
     * Saves and accepts a x509 certificate.
     *
     * @param cert
     */
    private void saveAndAcceptCert(X509Certificate cert) {
        android.util.Log.d(TAG, "Saving X509 cert fingerprint.");
        String certificate = null;
        try {
            certificate = Base64.encodeToString(cert.getEncoded(), Base64.DEFAULT);
        } catch (CertificateEncodingException e) {
            e.printStackTrace();
            c.showFatalMessageAndQuit(context.getString(R.string.error_x509_could_not_generate_encoding));
        }
        connection.setX509KeySignature(certificate);
        connection.setOvirtCaData(certificate);
        connection.save(context);

        // Indicate the certificate was accepted.
        c.rfbconn.setCertificateAccepted(true);
        synchronized (c.rfbconn) {
            c.rfbconn.notifyAll();
        }
        synchronized (RemoteCanvasHandler.this) {
            RemoteCanvasHandler.this.notifyAll();
        }
    }

    /**
     * Permits the user to validate a certificate with subject, issuer and fingerprint.
     *
     * @param subject     optional subject
     * @param issuer      optional issuer
     * @param fingerprint non-optional fingerprint that may be saved
     * @param save        whether to save the fingerprint and verify it if it is saved
     */
    public void validateCert(String subject, String issuer, final String fingerprint, boolean save) {
        boolean certMismatch = false;
        if (save) {
            if (connection.getX509KeySignature().equals(fingerprint)) {
                setCertificateAcceptedAndNotifyListeners();
                return;
            } else if (!connection.getX509KeySignature().equals("")) {
                certMismatch = true;
            }
        }
        // Show a dialog with the key signature for approval.
        DialogInterface.OnClickListener signatureNo = new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                // We were told not to continue, so stop the activity
                c.closeConnection();
                Utils.justFinish(context);
            }
        };
        DialogInterface.OnClickListener signatureYes = new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                // Indicate the certificate was accepted.
                if (save) {
                    connection.setX509KeySignature(fingerprint);
                    connection.save(context);
                }
                setCertificateAcceptedAndNotifyListeners();
            }
        };

        String message = context.getString(R.string.info_cert_signatures);
        if (!"".equals(subject)) {
            message += "\n" + context.getString(R.string.cert_subject) + ":     " + subject;
        }
        if (!"".equals(issuer)) {
            message += "\n" + context.getString(R.string.cert_issuer) + ":      " + issuer;
        }
        message += "\n" + context.getString(R.string.cert_fingerprint) + ": " + fingerprint +
                context.getString(R.string.info_cert_signatures_identical);
        if (certMismatch) {
            message += "\n\n" + context.getString(R.string.warning_cert_does_not_match);
        }
        Utils.showYesNoPrompt(context,
                context.getString(R.string.info_continue_connecting) + connection.getAddress() + "?",
                message,
                signatureYes,
                signatureNo
        );
    }

    private void setCertificateAcceptedAndNotifyListeners() {
        c.rfbconn.setCertificateAccepted(true);
        synchronized (c.rfbconn) {
            c.rfbconn.notifyAll();
        }
    }

    /**
     * Function used to initialize an empty SSH HostKey for a new VNC over SSH connection.
     */
    public void initializeSshHostKey() {
        // If the SSH HostKey is empty, then we need to grab the HostKey from the server and save it.
        Log.d(TAG, "Attempting to initialize SSH HostKey.");

        c.displayShortToastMessage(context.getString(R.string.info_ssh_initializing_hostkey));

        sshConnection = new SSHConnection(connection, context, this);
        if (!sshConnection.connect()) {
            // Failed to connect, so show error message and quit activity.
            c.showFatalMessageAndQuit(context.getString(R.string.error_ssh_unable_to_connect));
        } else {
            // Show a dialog with the key signature.
            DialogInterface.OnClickListener signatureNo = new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    // We were told to not continue, so stop the activity
                    sshConnection.terminateSSHTunnel();
                    c.pd.dismiss();
                    Utils.justFinish(context);
                }
            };
            DialogInterface.OnClickListener signatureYes = new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    // We were told to go ahead with the connection.
                    connection.setIdHash(sshConnection.getIdHash()); // could prompt based on algorithm
                    connection.setSshHostKey(sshConnection.getServerHostKey());
                    connection.save(context);
                    sshConnection.terminateSSHTunnel();
                    sshConnection = null;
                    synchronized (RemoteCanvasHandler.this) {
                        RemoteCanvasHandler.this.notify();
                    }
                }
            };

            Utils.showYesNoPrompt(context,
                    context.getString(R.string.info_continue_connecting) + connection.getSshServer() + "?",
                    context.getString(R.string.info_ssh_key_fingerprint) + sshConnection.getHostKeySignature() +
                            context.getString(R.string.info_ssh_key_fingerprint_identical),
                    signatureYes, signatureNo);
        }
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
                        connection.getSshUser(), connection.getSshPassword(), null,
                        connection.getKeepSshPassword());
                break;
            case RemoteClientLibConstants.GET_SSH_PASSPHRASE:
                showGetTextFragment(context.getString(R.string.ssh_passphrase_hint),
                        GetTextFragment.DIALOG_ID_GET_SSH_PASSPHRASE,
                        context.getString(R.string.enter_passphrase_title),
                        c.sshConnection, GetTextFragment.Password,
                        R.string.enter_passphrase, R.string.ssh_passphrase_hint,
                        null, null, null, connection.getKeepSshPassword());
                break;
            case RemoteClientLibConstants.GET_VNC_PASSWORD:
                showGetTextFragment(context.getString(R.string.enter_vnc_password),
                        GetTextFragment.DIALOG_ID_GET_VNC_PASSWORD,
                        context.getString(R.string.enter_vnc_password),
                        c, GetTextFragment.Password,
                        R.string.enter_vnc_password, R.string.enter_vnc_password,
                        connection.getPassword(), null, null, connection.getKeepPassword());
                break;
            case RemoteClientLibConstants.GET_VNC_CREDENTIALS:
                showGetTextFragment(context.getString(R.string.enter_vnc_credentials),
                        GetTextFragment.DIALOG_ID_GET_VNC_CREDENTIALS,
                        context.getString(R.string.enter_vnc_credentials),
                        c, GetTextFragment.Credentials,
                        R.string.enter_vnc_credentials, R.string.enter_vnc_credentials,
                        connection.getUserName(), connection.getPassword(), null,
                        connection.getKeepPassword());
                break;
            case RemoteClientLibConstants.GET_RDP_CREDENTIALS:
                showGetTextFragment(context.getString(R.string.enter_rdp_credentials),
                        GetTextFragment.DIALOG_ID_GET_RDP_CREDENTIALS,
                        context.getString(R.string.enter_rdp_credentials),
                        c, GetTextFragment.CredentialsWithDomain,
                        R.string.enter_rdp_credentials, R.string.enter_rdp_credentials,
                        connection.getUserName(), connection.getRdpDomain(), connection.getPassword(),
                        connection.getKeepPassword());
                break;
            case RemoteClientLibConstants.GET_SPICE_PASSWORD:
                showGetTextFragment(context.getString(R.string.enter_spice_password),
                        GetTextFragment.DIALOG_ID_GET_SPICE_PASSWORD,
                        context.getString(R.string.enter_spice_password),
                        c, GetTextFragment.Password,
                        R.string.enter_spice_password, R.string.enter_spice_password,
                        connection.getPassword(), null, null, connection.getKeepPassword());
                break;
            case RemoteClientLibConstants.DIALOG_SSH_CERT:
                android.util.Log.d(TAG, "DIALOG_SSH_CERT");
                initializeSshHostKey();
                break;
            case RemoteClientLibConstants.DIALOG_RDP_CERT:
                android.util.Log.d(TAG, "DIALOG_RDP_CERT");
                s = (Bundle) msg.obj;
                validateCert(
                        s.getString("subject"),
                        s.getString("issuer"),
                        s.getString("fingerprint"),
                        s.getBoolean("save", false)
                );
                break;
            case RemoteClientLibConstants.DISCONNECT_WITH_MESSAGE:
                c.showFatalMessageAndQuit(messageText);
                break;
            case RemoteClientLibConstants.SPICE_CONNECT_FAILURE_IF_MAINTAINING_CONNECTION:
                if (c.maintainConnection) {
                    c.showFatalMessageAndQuit(messageText);
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
                if (Utils.isOpaque(context)) {
                    c.reinitializeOpaque();
                } else {
                    c.reinitializeCanvas();
                }
                break;
            case RemoteClientLibConstants.REPORT_TOOLBAR_POSITION:
                android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION");
                if (connection.getUseLastPositionToolbar()) {
                    int useLastPositionToolbarX = Utils.getIntFromMessage(msg, "useLastPositionToolbarX");
                    int useLastPositionToolbarY = Utils.getIntFromMessage(msg, "useLastPositionToolbarY");
                    boolean useLastPositionToolbarMoved = Utils.getBooleanFromMessage(msg, "useLastPositionToolbarMoved");
                    android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION, X Coordinate" + useLastPositionToolbarX);
                    android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION, Y Coordinate" + useLastPositionToolbarY);
                    connection.setUseLastPositionToolbarX(useLastPositionToolbarX);
                    connection.setUseLastPositionToolbarY(useLastPositionToolbarY);
                    connection.setUseLastPositionToolbarMoved(useLastPositionToolbarMoved);
                }
                break;
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
                Utils.justFinish(context);
                break;
            case RemoteClientLibConstants.GET_PASSWORD:
                showGetTextFragmentRemoteCanvas(context.getString(R.string.enter_password),
                        GetTextFragment.DIALOG_ID_GET_OPAQUE_PASSWORD,
                        context.getString(R.string.enter_password),
                        c, GetTextFragment.Password,
                        R.string.enter_password, R.string.enter_password,
                        connection.getPassword(), null, null,
                        connection.getKeepPassword());
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
                } else {
                    c.maintainConnection = false;
                    showGetTextFragmentRemoteCanvas(context.getString(R.string.enter_password_auth_failed),
                            GetTextFragment.DIALOG_ID_GET_OPAQUE_CREDENTIALS,
                            context.getString(R.string.enter_password_auth_failed),
                            c, GetTextFragment.Credentials,
                            R.string.enter_password_auth_failed, R.string.enter_password_auth_failed,
                            connection.getUserName(), connection.getPassword(), null,
                            connection.getKeepPassword());
                    break;
                }
            case RemoteClientLibConstants.OVIRT_AUTH_FAILURE:
                if (c.retrievevvFileName() != null) {
                    c.disconnectAndShowMessage(R.string.error_ovirt_auth_failure, R.string.error_dialog_title);
                    break;
                } else {
                    c.maintainConnection = false;
                    showGetTextFragmentRemoteCanvas(context.getString(R.string.enter_password_auth_failed),
                            GetTextFragment.DIALOG_ID_GET_OPAQUE_CREDENTIALS,
                            context.getString(R.string.enter_password_auth_failed),
                            c, GetTextFragment.Credentials,
                            R.string.enter_password_auth_failed, R.string.enter_password_auth_failed,
                            connection.getUserName(), connection.getPassword(), null,
                            connection.getKeepPassword());
                    break;
                }
            case RemoteClientLibConstants.DIALOG_DISPLAY_VMS:
                c.pd.dismiss();
                ArrayList<String> vms = msg.getData().getStringArrayList("vms");

                if (vms.size() > 0) {
                    fm = ((FragmentActivity) context).getSupportFragmentManager();
                    SelectTextElementFragment displayVms = SelectTextElementFragment.newInstance(
                            context.getString(R.string.select_vm_title),
                            vms, (RemoteCanvasActivity) context);
                    displayVms.setCancelable(false);
                    displayVms.show(fm, "selectVm");
                }
                break;
            case RemoteClientLibConstants.SPICE_CONNECT_SUCCESS:
                if (c.pd != null && c.pd.isShowing()) {
                    c.pd.dismiss();
                }
                synchronized (c.spicecomm) {
                    c.spiceUpdateReceived = true;
                    c.spicecomm.notifyAll();
                }
                break;
            case RemoteClientLibConstants.SPICE_CONNECT_FAILURE:
                String e = msg.getData().getString("message");
                if (c.maintainConnection) {
                    c.maintainConnection = false;
                    if (c.pd != null && c.pd.isShowing()) {
                        c.pd.dismiss();
                    }
                    // Only if we were intending to stay connected, and the connection failed, show an error message.
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
                android.util.Log.d(TAG, "DIALOG_X509_CERT");
                X509Certificate cert = (X509Certificate) msg.obj;
                validateX509Cert(cert);
                break;
            case RemoteClientLibConstants.DISCONNECT_NO_MESSAGE:
                c.closeConnection();
                Utils.justFinish(context);
                break;
            case RemoteClientLibConstants.SHOW_TOAST:
                this.post(new Runnable() {
                    public void run() {
                        Toast.makeText(context, Utils.getStringResourceByName(context, messageText),
                                Toast.LENGTH_LONG).show();
                    }
                });
                break;
            default:
                android.util.Log.e(TAG, "Not handling unknown messageId: " + msg.what);
                break;
        }
    }
}
