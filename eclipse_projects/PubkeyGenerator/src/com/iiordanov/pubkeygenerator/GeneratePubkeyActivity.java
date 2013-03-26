/*
 * ConnectBot: simple, powerful, open-source SSH client for Android
 * Copyright 2012 Iordan Iordanov
 * Copyright 2007 Kenny Root, Jeffrey Sharkey
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.iiordanov.pubkeygenerator;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.Charset;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;

import com.iiordanov.pubkeygenerator.EntropyDialog;
import com.iiordanov.pubkeygenerator.EntropyView;
import com.iiordanov.pubkeygenerator.OnEntropyGatheredListener;
import com.iiordanov.pubkeygenerator.PubkeyDatabase;
import com.iiordanov.pubkeygenerator.PubkeyUtils;
import android.app.Activity;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Base64;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.Toast;
import android.widget.RadioGroup.OnCheckedChangeListener;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.text.ClipboardManager;

public class GeneratePubkeyActivity extends Activity implements OnEntropyGatheredListener {
	public final static String TAG = "GeneratePubkeyActivity";

	final static int MIN_BITS_RSA = 768;
	final static int DEFAULT_BITS_RSA = 2048;
	final static int MAX_BITS_RSA = 4096;
	final static int MIN_BITS_DSA = 512;
	final static int DEFAULT_BITS_DSA = 1024;
	final static int MAX_BITS_DSA = 1024;

	private LayoutInflater inflater = null;

	private RadioGroup keyTypeGroup;
	private SeekBar bitsSlider;
	private EditText bitsText;
	private Button generate;
	private Button importKey;
	private Button share;
	private Button decrypt;
	private Button copy;
	private Button save;
	private Dialog entropyDialog;
	private ProgressDialog progress;

	private EditText password1;
	private EditText file_name;

	private String keyType = PubkeyDatabase.KEY_TYPE_RSA;
	private int minBits = MIN_BITS_RSA;
	private int bits = DEFAULT_BITS_RSA;

	private byte[] entropy;
	
	// Variables we use to receive (from calling activity)
	// and recover all key-pair related information.
	private String passphrase;
	private String sshPrivKey;
	private String sshPubKey;
	private boolean recovered = false;
	private KeyPair kp = null;
	private String publicKeySSHFormat;
	
	ClipboardManager cm;

	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);

		setContentView(R.layout.act_generatepubkey);
		
		cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);

		keyTypeGroup = (RadioGroup) findViewById(R.id.key_type);

		bitsText = (EditText) findViewById(R.id.bits);
		bitsSlider = (SeekBar) findViewById(R.id.bits_slider);

		file_name = (EditText) findViewById(R.id.file_name);
		password1 = (EditText) findViewById(R.id.password);

		generate  = (Button) findViewById(R.id.generate);
		share     = (Button) findViewById(R.id.share);
		decrypt   = (Button) findViewById(R.id.decrypt);
		copy      = (Button) findViewById(R.id.copy);
		save      = (Button) findViewById(R.id.save);
		importKey = (Button) findViewById(R.id.importKey);
		
		inflater = LayoutInflater.from(this);

		password1.addTextChangedListener(textChecker);

		// Get the private key and passphrase from calling activity if added.
		sshPrivKey = getIntent().getStringExtra("PrivateKey");
		passphrase = password1.getText().toString();
		if (sshPrivKey != null && sshPrivKey.length() != 0) {
			decryptAndRecoverKey ();
		} else {
			Toast.makeText(getBaseContext(), "Key not generated yet. Set parameters and tap 'Generate New Key'.",
					Toast.LENGTH_LONG).show();
		}

		keyTypeGroup.setOnCheckedChangeListener(new OnCheckedChangeListener() {

			public void onCheckedChanged(RadioGroup group, int checkedId) {
				if (checkedId == R.id.rsa) {
					minBits = MIN_BITS_RSA;

					bitsSlider.setEnabled(true);
					bitsSlider.setProgress(DEFAULT_BITS_RSA);
					bitsSlider.setMax(MAX_BITS_RSA - minBits);

					bitsText.setText(String.valueOf(DEFAULT_BITS_RSA));
					bitsText.setEnabled(true);

					keyType = PubkeyDatabase.KEY_TYPE_RSA;
				} else if (checkedId == R.id.dsa) {
					minBits = MIN_BITS_DSA;

					bitsSlider.setEnabled(true);
					bitsSlider.setProgress(DEFAULT_BITS_DSA);
					bitsSlider.setMax(MAX_BITS_DSA - minBits);

					bitsText.setText(String.valueOf(DEFAULT_BITS_DSA));
					bitsText.setEnabled(true);

					keyType = PubkeyDatabase.KEY_TYPE_DSA;
				}
			}
		});

		bitsSlider.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {

			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromTouch) {
				// Stay evenly divisible by 8 because it looks nicer to have
				// 2048 than 2043 bits.

				int leftover = progress % 8;
				int ourProgress = progress;

				if (leftover > 0)
					ourProgress += 8 - leftover;

				bits = minBits + ourProgress;
				bitsText.setText(String.valueOf(bits));
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
				// We don't care about the start.
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
				// We don't care about the stop.
			}
		});

		bitsText.setOnFocusChangeListener(new OnFocusChangeListener() {
			public void onFocusChange(View v, boolean hasFocus) {
				if (!hasFocus) {
					try {
						bits = Integer.parseInt(bitsText.getText().toString());
						if (bits < minBits) {
							bits = minBits;
							bitsText.setText(String.valueOf(bits));
						}
					} catch (NumberFormatException nfe) {
						bits = DEFAULT_BITS_RSA;
						bitsText.setText(String.valueOf(bits));
					}

					bitsSlider.setProgress(bits - minBits);
				}
			}
		});

		generate.setOnClickListener(new OnClickListener() {
			public void onClick(View view) {
				hideSoftKeyboard(view);
				GeneratePubkeyActivity.this.startEntropyGather();
			}
		});

		decrypt.setOnClickListener(new OnClickListener() {
			public void onClick(View view) {
				hideSoftKeyboard(view);
				decryptAndRecoverKey();
			}
		});

		share.setOnClickListener(new OnClickListener() {
			public void onClick(View view) {
				hideSoftKeyboard(view);
				// This is an UGLY HACK for Blackberry devices which do not transmit the "+" character.
				// Remove as soon as the bug is fixed.
				String s = android.os.Build.MODEL;
				if (s.contains("BlackBerry")) {
					Toast.makeText(getBaseContext(), "ERROR: Blackberry devices have problems sharing public keys. " +
							"The '+' character is not transmitted. Please save as a file and attach in an email, or " +
							"copy to clipboard and paste when connected to the server with a password.",
							Toast.LENGTH_LONG).show();
					return;
				}
				Intent share = new Intent(Intent.ACTION_SEND);
				share.setType("text/plain");
				share.putExtra(Intent.EXTRA_TEXT, publicKeySSHFormat);
				startActivity(Intent.createChooser(share, "Share Pubkey"));
			}
		});
		
		copy.setOnClickListener(new OnClickListener() {
			public void onClick(View view) {
				hideSoftKeyboard(view);
				cm.setText(publicKeySSHFormat);
				Toast.makeText(getBaseContext(), "Copied public key in OpenSSH format to clipboard.",
						Toast.LENGTH_SHORT).show();				
			}
		});
		
		save.setOnClickListener(new OnClickListener() {
			public void onClick(View view) {
				hideSoftKeyboard(view);
				
				String fname = file_name.getText().toString();
				if (fname.length() == 0) {
					Toast.makeText(getBaseContext(), "Please enter file name.",
							Toast.LENGTH_SHORT).show();
					return;
				}
					
				File dir = Environment.getExternalStoragePublicDirectory (Environment.DIRECTORY_DOWNLOADS);
				File file = new File(dir, fname);
				fname = dir.getName() + "/" + fname;
				try {
			        dir.mkdirs();
					file.createNewFile();
					FileOutputStream fout = new FileOutputStream(file);
					OutputStreamWriter writer = new OutputStreamWriter(fout);
					writer.append(publicKeySSHFormat);
					writer.close();
					fout.close();
				} catch (IOException e) {
					Toast.makeText(getBaseContext(), "Failed to write " + fname,
							Toast.LENGTH_LONG).show();
					Log.e (TAG, "Failed to output file " + fname);
					e.printStackTrace();
					return;
				}

				Toast.makeText(getBaseContext(), "Successfully wrote public key in OpenSSH format to " + fname,
						Toast.LENGTH_LONG).show();
			}
		});
		
		importKey.setOnClickListener(new OnClickListener() {
			public void onClick(View view) {
				hideSoftKeyboard(view);
				
				String fname = file_name.getText().toString();
				if (fname.length() == 0) {
					Toast.makeText(getBaseContext(), "Please enter file name (at the bottom) to import PEM formatted " +
													 "encrypted/unencrypted RSA keys, PKCS#8 unencrypted DSA keys. " +
													 "Keys generated with 'ssh-keygen -t rsa' are known to work.", Toast.LENGTH_LONG).show();
					return;
				}
				
				File dir = Environment.getExternalStoragePublicDirectory (Environment.DIRECTORY_DOWNLOADS);
				fname = dir.getAbsolutePath() + "/" + fname;
				String data = "";
				try {
					data = readFile(fname);
				} catch (IOException e) {
					e.printStackTrace();
					Log.e (TAG, "Failed to read key from file: " + fname);
					Toast.makeText(getBaseContext(), "Failed to read file: " + fname + ". Please ensure it is present " +
													 "in Download directory.", Toast.LENGTH_LONG).show();
					return;
				}
				
				try {
					passphrase = password1.getText().toString();
					KeyPair pair = PubkeyUtils.tryImportingPemAndPkcs8(data, passphrase);
					converToBase64AndSendIntent (pair);
				} catch (Exception e) {
					e.printStackTrace();
					Log.e (TAG, "Failed to decode key.");
					Toast.makeText(getBaseContext(), e.getLocalizedMessage(), Toast.LENGTH_LONG).show();
					return;
				}
				Toast.makeText(getBaseContext(), "Successfully imported SSH key from file.", Toast.LENGTH_LONG).show();
				finish();
			}
		});
}
	
	/**
	 * Hides the soft keyboard.
	 */
	public void hideSoftKeyboard (View view) {
		InputMethodManager imm = (InputMethodManager)getSystemService(
			      Context.INPUT_METHOD_SERVICE);
			imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
	}
	
	/**
	 * Decrypts and recovers the key pair, as well as generating public key in OpenSSH format.
	 * @param view
	 */
	public boolean decryptAndRecoverKey () {
		boolean success = true;
		passphrase = password1.getText().toString();
		if (!recovered) {
			kp = PubkeyUtils.decryptAndRecoverKeyPair(sshPrivKey, passphrase);
			if (kp == null) {
				success = false;
			} else {
				try {
					publicKeySSHFormat = PubkeyUtils.convertToOpenSSHFormat(kp.getPublic(), null);
				} catch (Exception e) {
					e.printStackTrace();
					success = false;
				}
			}
			if (success)
				recovered = true;
		}

		if (recovered) {
			Toast.makeText(getBaseContext(), "Successfully decrypted key.", Toast.LENGTH_LONG).show();
		} else {
			Toast.makeText(getBaseContext(),
					"Could not decrypt key. Please enter correct passphrase and try decrypting again.",
					Toast.LENGTH_LONG).show();
		}
		checkEntries();
		return success;
	}

	/**
	 * Turns buttons on and off depending on state of pubkey.
	 */
	private void checkEntries() {
		if (recovered) {
			share.setEnabled(true);
			copy.setEnabled(true);
			save.setEnabled(true);
			decrypt.setEnabled(false);
		} else {
			share.setEnabled(false);
			copy.setEnabled(false);
			save.setEnabled(false);
			if (sshPrivKey.length() != 0)
				decrypt.setEnabled(true);		
		}
	}

	private void startEntropyGather() {
		final View entropyView = inflater.inflate(R.layout.dia_gatherentropy, null, false);
		((EntropyView)entropyView.findViewById(R.id.entropy)).addOnEntropyGatheredListener(GeneratePubkeyActivity.this);
		entropyDialog = new EntropyDialog(GeneratePubkeyActivity.this, entropyView);
		entropyDialog.show();
	}

	public void onEntropyGathered(byte[] entropy) {
		// For some reason the entropy dialog was aborted, exit activity
		if (entropy == null) {
			finish();
			return;
		}

		this.entropy = entropy.clone();

		int numSetBits = 0;
		for (int i = 0; i < 20; i++)
			numSetBits += measureNumberOfSetBits(this.entropy[i]);

		Log.d(TAG, "Entropy distribution=" + (int)(100.0 * numSetBits / 160.0) + "%");

		Log.d(TAG, "entropy gathered; attemping to generate key...");
		startKeyGen();
	}

	private void startKeyGen() {
		progress = new ProgressDialog(GeneratePubkeyActivity.this);
		progress.setMessage(getResources().getText(R.string.pubkey_generating));
		progress.setIndeterminate(true);
		progress.setCancelable(false);
		progress.show();

		Thread keyGenThread = new Thread(mKeyGen);
		keyGenThread.setName("KeyGen");
		keyGenThread.start();
	}

	private Handler handler = new Handler() {
		@Override
		public void handleMessage(Message msg) {
			progress.setMessage(getResources().getText(R.string.pubkey_generated));
			progress.dismiss();
			GeneratePubkeyActivity.this.finish();
		}
	};

	final private Runnable mKeyGen = new Runnable() {
		public void run() {
			try {
				SecureRandom random = SecureRandom.getInstance("SHA1PRNG");
				random.setSeed(entropy);

				KeyPairGenerator keyPairGen = KeyPairGenerator.getInstance(keyType);
				keyPairGen.initialize(bits, random);

				KeyPair pair = keyPairGen.generateKeyPair();
				converToBase64AndSendIntent (pair);

			} catch (Exception e) {
				Log.e(TAG, "Could not generate key pair");
				e.printStackTrace();
			}

			handler.sendEmptyMessage(0);
		}

	};

	final private TextWatcher textChecker = new TextWatcher() {
		public void afterTextChanged(Editable s) {}

		public void beforeTextChanged(CharSequence s, int start, int count,
				int after) {}

		public void onTextChanged(CharSequence s, int start, int before,
				int count) {
			checkEntries();
		}
	};

	private int measureNumberOfSetBits(byte b) {
		int numSetBits = 0;

		for (int i = 0; i < 8; i++) {
			if ((b & 1) == 1)
				numSetBits++;
			b >>= 1;
		}

		return numSetBits;
	}
	
	private static String readFile(String path) throws IOException {
		FileInputStream stream = new FileInputStream(new File(path));
		try {
			FileChannel fc = stream.getChannel();
			MappedByteBuffer bb = fc.map(FileChannel.MapMode.READ_ONLY, 0, fc.size());
			/* Instead of using default, pass in a decoder. */
			return Charset.defaultCharset().decode(bb).toString();
		}
		finally {
			stream.close();
		}
	}
	
	/**
	 * Sets the sshPrivKey and sshPubKey private variables from provided KeyPair.
	 * @param pair
	 * @throws Exception
	 */
	private void converToBase64AndSendIntent (KeyPair pair) throws Exception {
		PrivateKey priv = pair.getPrivate();
		PublicKey pub = pair.getPublic();

		String secret = password1.getText().toString();
		Log.d(TAG, "private: " + PubkeyUtils.formatKey(priv));
		Log.d(TAG, "public: " + PubkeyUtils.formatKey(pub));
		sshPrivKey = Base64.encodeToString(PubkeyUtils.getEncodedPrivate(priv, secret), Base64.DEFAULT);
		sshPubKey  = Base64.encodeToString(PubkeyUtils.getEncodedPublic(pub), Base64.DEFAULT);

		// Send the generated data back to the calling activity.
		Intent databackIntent = new Intent();
		databackIntent.putExtra("PrivateKey", sshPrivKey);
		databackIntent.putExtra("PublicKey", sshPubKey);

		setResult(Activity.RESULT_OK, databackIntent);
	}
}
