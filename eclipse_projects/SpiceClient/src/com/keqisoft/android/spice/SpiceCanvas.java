package com.keqisoft.android.spice;

import com.keqisoft.android.spice.datagram.BitmapDG;

import android.content.Context;
import android.graphics.Bitmap;
//import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.widget.ImageView;

public class SpiceCanvas extends ImageView {
	public static final int UPDATE_CANVAS = 111;
	public static final int NEW_CANVAS_SIZE = 222;

	private BitmapDG bitmapDg = new BitmapDG();
	private Matrix matrix;
	private int x;
	private int y;
	private int displayWidth, displayHeight;
	private Paint paint = new Paint();

	public void setDisplayWH(int w, int h) {
		this.displayWidth = w;
		this.displayHeight = h;
	}

	public BitmapDG getBitmapDG() {
		return bitmapDg;
	}

	public int getXOffset() {
		return x;
	}

	public int getYOffset() {
		return y;
	}

	public SpiceCanvas(Context context, AttributeSet attrs) {
		super(context, attrs);
		matrix = new Matrix();
		setFocusable(true);
	    setFocusableInTouchMode(true);
//		bitmapDg = new BitmapDG();
//		bitmapDg.setBitmap(BitmapFactory.decodeFile("/data/local/tmp/1.JPG"));
//		bitmapDg.setW(bitmapDg.getBitmap().getWidth());
//		bitmapDg.setH(bitmapDg.getBitmap().getHeight());
	}

	@Override
	public void onDraw(Canvas canvas) {
		if (bitmapDg.getBitmap() != null) {
			Bitmap bm = Bitmap.createBitmap(bitmapDg.getBitmap(), 0, 0,
					bitmapDg.getBitmap().getWidth(), bitmapDg.getBitmap()
							.getHeight(), matrix, true);
			canvas.drawBitmap(bm, 0, 0, paint);
		}
	}

	/**
	 * 放大或缩小
	 * 
	 * @param activity
	 */
	public void zoom(float scaling) {
		resetMatrix();
		matrix.postScale(scaling, scaling);
		setImageMatrix(matrix);
		pan(0, 0, scaling);
		invalidate();
	}

	/**
	 * 平移x与y
	 * 
	 * @param x
	 * @param y
	 */
	public void pan(int px, int py, float scaling) {
		x += px;
		y += py;
		calcBorder(scaling);
//		Log.v("keqisoft", "PAN->(x,y)=(" + x + "," + y + "),(px,py)=(" + px + "," + py+"),H=" + bitmapDg.getH());
		scrollTo(x, y);
	}

	/**
	 * 平移到某个位置
	 */
	public void panTo(int px, int py, float scaling) {
		x = px;
		y = py;
		calcBorder(scaling);
		scrollTo(x, y);
	}

	private int blackSpaceSize = 40;
	/**
	 * 计算是否需要平移到合适的范围之内
	 * 
	 * @param scaling
	 * @return
	 */
	private void calcBorder(float scaling) {
		if (x < - blackSpaceSize) {
			x = - blackSpaceSize;
		} else 		
		if (bitmapDg != null
				&& (x + displayWidth) > bitmapDg.getW() * scaling + blackSpaceSize) {
			x = (int) (bitmapDg.getW() * scaling) - displayWidth + blackSpaceSize;
		}
		
		if (y < - blackSpaceSize) {
			y = - blackSpaceSize;
		} else 
		if (bitmapDg != null
				&& (y + displayHeight) > bitmapDg.getH() * scaling + blackSpaceSize) {
			y = (int) (bitmapDg.getH() * scaling) - displayHeight + blackSpaceSize;
		}
	}

	/**
	 * 
	 */
	private void resetMatrix() {
		matrix.reset();
		matrix.preTranslate(0, 0);
	}
}
