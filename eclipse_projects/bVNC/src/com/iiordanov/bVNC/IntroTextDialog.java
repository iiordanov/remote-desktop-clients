/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
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
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.database.sqlite.SQLiteDatabase;
import android.os.Bundle;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.MenuItem.OnMenuItemClickListener;
import android.widget.Button;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

/**
 * @author Michael A. MacDonald
 *
 */
class IntroTextDialog extends Dialog {

	private PackageInfo packageInfo;
	private VncDatabase database;
	
	static IntroTextDialog dialog = null;
	
	private boolean donate = false;
	
	static void showIntroTextIfNecessary(Activity context, VncDatabase database, boolean show) {
		PackageInfo pi;
		try {
			pi = context.getPackageManager().getPackageInfo("com.iiordanov.bVNC", 0);
		}
		catch (PackageManager.NameNotFoundException nnfe) {
			return;
		}
		MostRecentBean mr = bVNC.getMostRecent(database.getReadableDatabase());
		
		if (dialog == null && show && (mr == null || mr.getShowSplashVersion() != pi.versionCode)) {
			dialog = new IntroTextDialog(context, pi, database);
			dialog.show();
		}
	}
	
	/**
	 * @param context -- Containing dialog
	 */
	private IntroTextDialog(Activity context, PackageInfo pi, VncDatabase database) {
		super(context);
		setOwnerActivity(context);
		packageInfo = pi;
		this.database = database;
	}

	/* (non-Javadoc)
	 * @see android.app.Dialog#onCreate(android.os.Bundle)
	 */
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		String pkgName = this.getContext().getPackageName();
		if (pkgName.contains("free") || ! (pkgName.contains("bVNC")))
			donate = true;
				
		setContentView(R.layout.intro_dialog);
		getWindow().setLayout(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT);

		StringBuilder sb = new StringBuilder(getContext().getResources().getString(R.string.intro_title));
		setTitle(sb);
		sb.delete(0, sb.length());
		if (donate) {
			sb.append("<a href=\"market://details?id=com.iiordanov.bVNC\">" + 
					getContext().getResources().getString(R.string.ad_donate_text) + "</a>");
			sb.append("<br>");
			sb.append("<br>");
			sb.append("<a href=\"market://search?q=undatech\">" + 
					getContext().getResources().getString(R.string.ad_donate_text2) + "</a>");
			sb.append("<br>");
			sb.append("<br>");
		}
		sb.append(getContext().getResources().getString(R.string.intro_header));
		sb.append(getContext().getResources().getString(R.string.intro_text));
		sb.append("\n");
		sb.append(getContext().getResources().getString(R.string.intro_version_text));
		TextView introTextView = (TextView)findViewById(R.id.textIntroText);
		introTextView.setText(Html.fromHtml(sb.toString()));
		introTextView.setMovementMethod(LinkMovementMethod.getInstance());
		((Button)findViewById(R.id.buttonCloseIntro)).setOnClickListener(new View.OnClickListener() {

			/* (non-Javadoc)
			 * @see android.view.View.OnClickListener#onClick(android.view.View)
			 */
			@Override
			public void onClick(View v) {
				showAgain(true);
			}
			
		});
			
		Button buttonCloseIntroDontShow = (Button)findViewById(R.id.buttonCloseIntroDontShow);
		if (donate) {
			buttonCloseIntroDontShow.setVisibility(View.GONE);
		} else {
			buttonCloseIntroDontShow.setOnClickListener(new View.OnClickListener() {

				/* (non-Javadoc)
				 * @see android.view.View.OnClickListener#onClick(android.view.View)
				 */
				@Override
				public void onClick(View v) {
					showAgain(false);
				}
				
			});
		}
	}

	/* (non-Javadoc)
	 * @see android.app.Dialog#onCreateOptionsMenu(android.view.Menu)
	 */
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		super.onCreateOptionsMenu(menu);
		getOwnerActivity().getMenuInflater().inflate(R.menu.intro_dialog_menu,menu);
		// Disabling Manual/Wiki Menu item as the original does not correspond to this project anymore.
		/*
		menu.findItem(R.id.itemOpenDoc).setOnMenuItemClickListener(new OnMenuItemClickListener() {

			@Override
			public boolean onMenuItemClick(MenuItem item) {
				Utils.showDocumentation(getOwnerActivity());
				dismiss();
				return true;
			}
		});
		*/
		menu.findItem(R.id.itemClose).setOnMenuItemClickListener(new OnMenuItemClickListener() {

			@Override
			public boolean onMenuItemClick(MenuItem item) {
				showAgain(true);
				return true;
			}
		});
		menu.findItem(R.id.itemDontShowAgain).setOnMenuItemClickListener(new OnMenuItemClickListener() {

			@Override
			public boolean onMenuItemClick(MenuItem item) {
				showAgain(false);
				return true;
			}
		});
		return true;
	}
	
	/* 
	 * (non-Javadoc)
	 * @see android.app.Dialog#onBackPressed()
	 */
	@Override
	public void onBackPressed () {
		showAgain(true);		
	}
	
	private void showAgain(boolean show) {
		SQLiteDatabase db = database.getWritableDatabase();
		MostRecentBean mostRecent = bVNC.getMostRecent(db);
		if (mostRecent != null) {
			int value = -1;
			if (!show) {
				value = packageInfo.versionCode;
			}
			mostRecent.setShowSplashVersion(value);
			mostRecent.Gen_update(db);
		}
		dismiss();
		dialog = null;
	}
}
