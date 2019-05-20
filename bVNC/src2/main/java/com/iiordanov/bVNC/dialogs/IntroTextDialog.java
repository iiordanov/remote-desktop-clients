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

package com.iiordanov.bVNC.dialogs;

import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.Database;

import android.app.Activity;
import android.app.Dialog;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import net.sqlcipher.database.SQLiteDatabase;
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
import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;

/**
 * @author Michael A. MacDonald
 *
 */
public class IntroTextDialog extends Dialog {

    private PackageInfo packageInfo;
    private Database database;
    
    static IntroTextDialog dialog = null;
    
    private boolean donate = false;
    
    public static void showIntroTextIfNecessary(Activity context, Database database, boolean show) {
        PackageInfo pi;
        try {
            pi = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
        }
        catch (PackageManager.NameNotFoundException nnfe) {
            return;
        }
        MostRecentBean mr = ConnectionBean.getMostRecent(database.getReadableDatabase());
        database.close();
        
        if (dialog == null && show && (mr == null || mr.getShowSplashVersion() != pi.versionCode)) {
            dialog = new IntroTextDialog(context, pi, database);
            dialog.show();
        }
    }
    
    /**
     * @param context -- Containing dialog
     */
    private IntroTextDialog(Activity context, PackageInfo pi, Database database) {
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
        if (pkgName.contains("free")) {
            donate = true;
        }
        
        setContentView(R.layout.intro_dialog);
        getWindow().setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        
        String packageName = getContext().getPackageName();
        String title = getContext().getResources().getString(R.string.intro_title);
        if (Utils.isRdp(packageName)) {
            title = getContext().getResources().getString(R.string.rdp_intro_title);
        } else if (Utils.isSpice(packageName)) {
            title = getContext().getResources().getString(R.string.spice_intro_title);
        }
        StringBuilder sb = new StringBuilder(title);
        setTitle(sb);
        sb.delete(0, sb.length());
        if (pkgName.contains("SPICE")) {
            sb.append(getContext().getResources().getString(R.string.ad_donate_text_spice));
            sb.append("<br>");
            sb.append("<br>");
        } else if (pkgName.contains("RDP")) {
            sb.append(getContext().getResources().getString(R.string.ad_donate_text_rdp));
            sb.append("<br>");
            sb.append("<br>");
        }
        sb.append(getContext().getResources().getString(R.string.ad_donate_text0));
        sb.append("<br>");
        sb.append("<br>");
        
        String donationPackageName = Utils.getDonationPackageName(getContext());
        if (donate) {
            sb.append("<a href=\"market://DETAILS?id=" + donationPackageName + "\">" +
                      getContext().getResources().getString(R.string.ad_donate_text1) + "</a>");
            sb.append("<br>");
            sb.append("<br>");
            sb.append(getContext().getResources().getString(R.string.ad_donate_text2));
            sb.append("<br>");
            sb.append("<br>");
            sb.append(getContext().getResources().getString(R.string.ad_donate_text3));
            sb.append(" <a href=\"market://details?id=com.iiordanov.bVNC\">VNC</a>");
            sb.append(", ");
            sb.append("<a href=\"market://details?id=com.iiordanov.aRDP\">RDP</a>");
            sb.append(", ");
            sb.append("<a href=\"market://details?id=com.iiordanov.aSPICE\">SPICE</a>");
            sb.append(", ");
            sb.append("<a href=\"market://details?id=com.undatech.opaque\">oVirt/RHEV</a>");
            sb.append("<br>");
            sb.append("<br>");
        }
        
        sb.append(getContext().getResources().getString(R.string.intro_header));
        if (Utils.isVnc(packageName)) {
            sb.append(getContext().getResources().getString(R.string.intro_text));
        } else if (Utils.isRdp(packageName)) {
            sb.append(getContext().getResources().getString(R.string.rdp_intro_text));
        } else if (Utils.isSpice(packageName)) {
            sb.append(getContext().getResources().getString(R.string.spice_intro_text));
        }
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
        MostRecentBean mostRecent = ConnectionBean.getMostRecent(db);
        if (mostRecent != null) {
            int value = -1;
            if (!show) {
                value = packageInfo.versionCode;
            }
            mostRecent.setShowSplashVersion(value);
            mostRecent.Gen_update(db);
        }
        database.close();
        dismiss();
        dialog = null;
    }
}
