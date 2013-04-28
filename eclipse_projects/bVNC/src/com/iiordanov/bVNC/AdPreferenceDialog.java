/**
 * Copyright (C) 2012 Iordan Iordanov
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

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.database.sqlite.SQLiteDatabase;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.MenuItem.OnMenuItemClickListener;
import android.widget.Button;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;


class AdPreferenceDialog extends Dialog {
	
	private Context context = null;
	static AdPreferenceDialog dialog = null;
	
	static void showDialogIfNecessary(Activity context, boolean show) {
		if (dialog == null && show) {
			dialog = new AdPreferenceDialog(context);
			dialog.show();
		}
	}
	
	/**
	 * @param context -- Containing dialog
	 */
	private AdPreferenceDialog(Activity context) {
		super(context);
		setOwnerActivity(context);
		this.context = context;
	}

	/* (non-Javadoc)
	 * @see android.app.Dialog#onCreate(android.os.Bundle)
	 */
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		setContentView(R.layout.intro_dialog);
		getWindow().setLayout(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT);
		
		StringBuilder sb = new StringBuilder(getContext().getResources().getString(R.string.ad_title));
		setTitle(sb);
		sb.delete(0, sb.length());
		sb.append("<a href=\"market://details?id=com.iiordanov.bVNC\">" + 
					getContext().getResources().getString(R.string.ad_donate_text) + "</a>");
		sb.append("<br>");
		sb.append(getContext().getResources().getString(R.string.ad_text));
		TextView introTextView = (TextView)findViewById(R.id.textIntroText);
		introTextView.setText(Html.fromHtml(sb.toString()));
		introTextView.setMovementMethod(LinkMovementMethod.getInstance());
		
		Button close = (Button)findViewById(R.id.buttonCloseIntro);
		close.setText(R.string.ad_no_ads);
		close.setOnClickListener(new View.OnClickListener() {

			/* (non-Javadoc)
			 * @see android.view.View.OnClickListener#onClick(android.view.View)
			 */
			@Override
			public void onClick(View v) {
				showAgain(true);
			}
			
		});
		Button showAd = (Button)findViewById(R.id.buttonCloseIntroDontShow);
		showAd.setText(R.string.ad_show_ads);
		showAd.setOnClickListener(new View.OnClickListener() {

			/* (non-Javadoc)
			 * @see android.view.View.OnClickListener#onClick(android.view.View)
			 */
			@Override
			public void onClick(View v) {
				showAgain(false);
			}
		});

	}

	/* (non-Javadoc)
	 * @see android.app.Dialog#onCreateOptionsMenu(android.view.Menu)
	 */
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		return false;
	}
	
	private void showAgain(boolean show) {
		Editor editor = PreferenceManager.getDefaultSharedPreferences(context).edit();
        editor.putBoolean("showAds", !show);              
        editor.commit();
        dismiss();
		dialog = null;
	}
}
