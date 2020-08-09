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
import java.util.HashMap;
import java.util.Map;

import android.app.Activity;
import android.content.Context;
import android.database.DataSetObserver;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.AppCompatImageView;
import android.util.Log;
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
    private String defaultLabel = "Untitled";
    Map<String, Connection> filteredConnectionsByPosition = new HashMap<>();
    String[] filter = null;

    public LabeledImageApapter(Context context, Map<String, Connection> connectionsByPosition, String[] filter, int numCols) {
        this.context = context;
        this.numCols = numCols;
        this.filter = filter;
        int i = 0;
        if (connectionsByPosition != null) {
            for (Connection c : connectionsByPosition.values()) {
                boolean include = true;
                for (String item : this.filter) {
                    if (!c.getLabel().toLowerCase().contains(item)) {
                        include = false;
                    }
                }
                if (include) {
                    this.filteredConnectionsByPosition.put(Integer.toString(i), c);
                    i++;
                }
            }
        }
    }
 
    public View getView(int position, View convertView, ViewGroup parent) {
        LayoutInflater inflater = (LayoutInflater)context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        Connection c = filteredConnectionsByPosition.get(Integer.toString(position));

        String label = c.getLabel();
        android.util.Log.d(TAG, "Now setting label at position: " + position + " to: " + label);

        GridView gView = (GridView) ((Activity)context).findViewById(R.id.gridView);
        int height = gView.getWidth()/numCols;
        
        View gridView;
        if (convertView != null) {
            gridView = convertView;
            AppCompatImageView iView = convertView.findViewById(R.id.grid_item_image);
            Drawable tempDrawable = iView.getDrawable();
            if (tempDrawable instanceof BitmapDrawable) {
                Bitmap tmp = ((BitmapDrawable) tempDrawable).getBitmap();
                if (tmp != null) {
                    tmp.recycle();
                }
            }
        } else {
            gridView = inflater.inflate(R.layout.grid_item, null);
        }

        GridView.LayoutParams lp = new GridView.LayoutParams(height, height);
        gridView.setLayoutParams(lp);

        TextView textView = (TextView) gridView.findViewById(R.id.grid_item_text);
        if ("".equals(label)) {
            textView.setText(defaultLabel);
        } else {
            textView.setText(label);
        }
        String screenshotFilePath = context.getFilesDir() + "/" + c.getScreenshotFilename();
        AppCompatImageView imageView = gridView.findViewById(R.id.grid_item_image);
        if (new File(screenshotFilePath).exists()) {
            Log.d(TAG, "Setting screenshot from file " + screenshotFilePath);
            Bitmap gridImage = BitmapFactory.decodeFile(screenshotFilePath);
            imageView.setImageBitmap(gridImage);
        } else {
            imageView.setImageResource(R.drawable.ic_screen_black_48dp);
            imageView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        }

        // This ID in a hidden TextView is used to send the right connection for editing or connection establishment
        // when the item is long-tapped or tapped respectively.
        TextView gridItemId = (TextView) gridView.findViewById(R.id.grid_item_id);
        gridItemId.setText(c.getRuntimeId());

        return gridView;
    }
 
    @Override
    public int getCount() {
        int count = 0;
        if (filteredConnectionsByPosition != null) {
            count = filteredConnectionsByPosition.size();
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
