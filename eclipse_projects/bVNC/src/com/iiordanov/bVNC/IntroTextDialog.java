/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (c) 2010 Michael A. MacDonald
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
import android.widget.TextView;

/**
 * @author Michael A. MacDonald
 *
 */
class IntroTextDialog extends Dialog {

	private PackageInfo packageInfo;
	private VncDatabase database;
	
	static IntroTextDialog dialog;
	
	static void showIntroTextIfNecessary(Activity context, VncDatabase database)
	{
		PackageInfo pi;
		try
		{
			pi = context.getPackageManager().getPackageInfo("com.iiordanov.bVNC", 0);
		}
		catch (PackageManager.NameNotFoundException nnfe)
		{
			return;
		}
		MostRecentBean mr = androidVNC.getMostRecent(database.getReadableDatabase());
		if (mr == null || mr.getShowSplashVersion() != pi.versionCode)
		{
			if (dialog == null)
			{
				dialog = new IntroTextDialog(context, pi, database);
				dialog.show();
			}
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
		setContentView(R.layout.intro_dialog);
		StringBuilder sb = new StringBuilder(getContext().getResources().getString(R.string.intro_title));
		//sb.append(" ");
		//sb.append(packageInfo.versionName);
		setTitle(sb);
		sb.delete(0, sb.length());
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
				dismiss();
			}
			
		});
		((Button)findViewById(R.id.buttonCloseIntroDontShow)).setOnClickListener(new View.OnClickListener() {

			/* (non-Javadoc)
			 * @see android.view.View.OnClickListener#onClick(android.view.View)
			 */
			@Override
			public void onClick(View v) {
				dontShowAgain();
			}
			
		});

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
				dismiss();
				return true;
			}
		});
		menu.findItem(R.id.itemDontShowAgain).setOnMenuItemClickListener(new OnMenuItemClickListener() {

			@Override
			public boolean onMenuItemClick(MenuItem item) {
				dontShowAgain();
				return true;
			}
		});
		return true;
	}

	private void dontShowAgain()
	{
		SQLiteDatabase db = database.getWritableDatabase();
		MostRecentBean mostRecent = androidVNC.getMostRecent(db);
		if (mostRecent != null)
		{
			mostRecent.setShowSplashVersion(packageInfo.versionCode);
			mostRecent.Gen_update(db);
		}
		dismiss();
	}
}
