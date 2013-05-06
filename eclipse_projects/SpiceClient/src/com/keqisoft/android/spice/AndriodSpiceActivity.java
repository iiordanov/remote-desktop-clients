package com.keqisoft.android.spice;

import java.util.StringTokenizer;

import com.keqisoft.android.spice.socket.Connector;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

public class AndriodSpiceActivity extends Activity {
	private TextView ipText;
	private TextView portText;
	private TextView passwordText;
	private TextView resultText;
	private Button connectBtn;
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.androidspice);
        
        ipText = (TextView)findViewById(R.id.ipText);
        portText = (TextView)findViewById(R.id.portText);
        passwordText = (TextView)findViewById(R.id.passwordText);
        resultText = (TextView)findViewById(R.id.resultText);
        resultText.setTextColor(Color.RED);
        connectBtn = (Button)findViewById(R.id.connectBtn);
        
        ipText.setText("192.168.2.20");
        portText.setText("5902");
        passwordText.setText("qweasdzx");
        
        connectBtn.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View arg0) {
				String ip = ipText.getText().toString();
				if (ip.length() == 0) {
					resultText.setText(R.string.error_null_ip);
					return;
				}
				if (!isIP(ip)) {
					resultText.setText(R.string.error_invalid_ip_port);
					return;
				}
				String port = portText.getText().toString();
				if (port.length() == 0) {
					resultText.setText(R.string.error_null_port);
					return;
				}
				String password = passwordText.getText().toString();
				if (password.length() == 0) {
					resultText.setText(R.string.error_null_password);
					return;
				}
				
				if (startSpice(ip,port,password)) {
				    toCanvas();
				}
			}
		});
    }
    
    private boolean startSpice(String ip,String port,String password) {
    	int rs = Connector.getInstance().connect(ip, port, password);
    	switch (rs) {
		case Connector.CONNECT_IP_PORT_ERROR:
			resultText.setText(R.string.error_invalid_ip_port);
			return false;
		case Connector.CONNECT_PASSWORD_ERROR:
			resultText.setText(R.string.error_invalid_password);
			return false;
		case Connector.CONNECT_UNKOWN_ERROR:
			resultText.setText(R.string.error_connect_failed);
			return false;
		default:
			break;
		}
    	return true;
    }
    
    private void toCanvas() {
    	Intent intent = new Intent(this, SpiceCanvasActivity.class);
		startActivity(intent);
    }
    
    private boolean isIP(String ip) {
		if (ip == null) {
			return false;
		}
		StringTokenizer tkn = new StringTokenizer(ip, ".");
		int c = tkn.countTokens();
		if (c != 4) {
			return false;
		}
		for (int i = 0; i < 4; i++) {
			try {
				Integer.parseInt(tkn.nextToken());
			} catch (NumberFormatException e) {
				return false;
			}
		}
		return true;
	}
}
