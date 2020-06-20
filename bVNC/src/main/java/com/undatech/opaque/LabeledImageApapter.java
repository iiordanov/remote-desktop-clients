/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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


package com.undatech.opaque;

import java.io.File;
import java.util.Map;

import android.app.Activity;
import android.content.Context;
import android.database.DataSetObserver;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.GridView;
import android.widget.ListAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import com.undatech.remoteClientUi.R;

public class LabeledImageApapter implements ListAdapter {
    private static final String TAG = "LabeledImageApapter";
    
    private Context context;
    private int numCols = 0;
    private Bitmap defaultBitmap = null;
    private String defaultLabel = null;
    Map<String, Connection> getConnectionsById = null;

    public LabeledImageApapter(Context context, Map<String, Connection> getConnectionsById, int numCols) {
        this.context = context;
        this.numCols = numCols;
        this.defaultBitmap = BitmapFactory.decodeResource(context.getResources(), R.drawable.no_screenshot);
        this.defaultLabel = "(No VM specified)";
        this.getConnectionsById = getConnectionsById;
    }
 
    public View getView(int position, View convertView, ViewGroup parent) {
        LayoutInflater inflater = (LayoutInflater)context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        android.util.Log.d(TAG, "Now setting label at position: " + position + " to: " + getConnectionsById.get(Integer.toString(position)).getLabel());

        GridView gView = (GridView) ((Activity)context).findViewById(R.id.gridView);
        int height = gView.getWidth()/numCols;
        
        View gridView;
        if (convertView != null) {
            gridView = convertView;
            ImageView iView = (ImageView) convertView.findViewById(R.id.grid_item_image);
            Bitmap tmp = ((BitmapDrawable)iView.getDrawable()).getBitmap();
            if (tmp != null && !tmp.equals(defaultBitmap)) {
                tmp.recycle();
            }
        } else {
            gridView = new View(context); 
            gridView = inflater.inflate(R.layout.grid_item, null);
        }
        
        GridView.LayoutParams lp = new GridView.LayoutParams(height, height);
        gridView.setLayoutParams(lp);

        TextView textView = (TextView) gridView.findViewById(R.id.grid_item_text);
        if (getConnectionsById.get(Integer.toString(position)).getLabel().equals("")) {
            textView.setText(defaultLabel);
        } else {
            textView.setText(getConnectionsById.get(Integer.toString(position)).getLabel());
        }
        ImageView imageView = (ImageView) gridView.findViewById(R.id.grid_item_image);
        if (new File(getConnectionsById.get(Integer.toString(position)).getFilename()).exists()) {
            Bitmap gridImage = BitmapFactory.decodeFile(getConnectionsById.get(Integer.toString(position)).getFilename());
            imageView.setImageBitmap(gridImage);
        } else {
            imageView.setImageBitmap(defaultBitmap);
        }
        
        return gridView;
    }
 
    @Override
    public int getCount() {
        int count = 0;
        if (getConnectionsById != null) {
            count = getConnectionsById.size();
        }
        return count;
    }
 
    @Override
    public Object getItem(int position) {
        return null;
    }
 
    @Override
    public long getItemId(int position) {
        return position;
    }
    
    @Override
    public boolean areAllItemsEnabled () {
        return true;
    }
    
    @Override
    public boolean hasStableIds () {
        return true;
    }

    @Override
    public int getItemViewType(int position) {
        return 0;
    }

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public boolean isEmpty() {
        return false;
    }

    @Override
    public void registerDataSetObserver(DataSetObserver observer) {
    }

    @Override
    public void unregisterDataSetObserver(DataSetObserver observer) {
    }

    @Override
    public boolean isEnabled(int position) {
        return true;
    }
}