package com.undatech.opaque;

import android.content.Context;
import android.os.Handler;
import android.os.Message;
import com.undatech.opaque.R;

public class RemoteCanvasActivityHandler extends Handler {
    private Context context;
    
    public RemoteCanvasActivityHandler(Context context) {
        super();
        this.context = context;
    }
    
    @Override
    public void handleMessage(Message msg) {
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
                                                   R.string.error_dialog_title);
            break;
        case Constants.PVE_API_IO_ERROR:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_io_error,
                                                   R.string.error_dialog_title);
            break;
        case Constants.PVE_TIMEOUT_COMMUNICATING:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_timeout_communicating,
                                                   R.string.error_dialog_title);
            break;
        case Constants.PVE_NULL_DATA:
            MessageDialogs.displayMessageAndFinish(context, R.string.error_pve_null_data,
                                                   R.string.error_dialog_title);
            break;
        }
    }

}
