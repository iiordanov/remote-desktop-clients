package com.keqisoft.android.spice.ppm;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;

public class PPMLoader {
	private PPM ppm = new PPM();

	public Bitmap load(String filePath) {
		BufferedInputStream in = null;
		try {
			in = new BufferedInputStream(
					new FileInputStream(new File(filePath)));
			byte[] bs = new byte[16];
			in.read(bs);

			ppm.setStyle(new String(bs, 0, 2));
			ppm.setWidth(Integer.valueOf(new String(bs, 3, 4)));
			ppm.setHeight(Integer.valueOf(new String(bs, 8, 3)));
			ppm.setMax(Integer.valueOf(new String(bs, 12, 3)));
			int[] colors = new int[ppm.getWidth() * ppm.getHeight()];

			bs = new byte[64];
			int len = 0;
			int cnt = 0;
			int total = 0;
			int[] rgb = new int[3];
			while ((len = in.read(bs)) > 0) {
				for (int i = 0; i < len; i ++) {
					rgb[cnt] = bs[i]>=0?bs[i]:(bs[i] + 255);
					if ((++cnt) == 3) {
						cnt = 0;
						colors[total++] = Color.rgb(rgb[0], rgb[1], rgb[2]);
					}
				}
			}
			return Bitmap.createBitmap(colors, ppm.getWidth(), ppm.getHeight(), Config.ARGB_8888);
		} catch (IOException e) {
		} finally {
			if (in != null) {
				try {
					in.close();
				} catch (IOException e) {
				}
			}
		}
		return null;
	}
}
