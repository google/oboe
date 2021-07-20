package com.google.oboe.samples.soundboard;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PointF;
import android.graphics.Rect;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;

import static android.os.Process.getExclusiveCores;

public class MusicTileView extends View {
    private ArrayList<Rect> mRectangles;
    private boolean[] mIsPressedPerRectangle;
    private Paint mPaint;
    private SparseArray<PointF> mLocationsOfFingers;

    private long mEngineHandle = 0;
    private native void tap(long engineHandle, boolean isDown, int audioSource);

    public MusicTileView(Context context, ArrayList<Rect> rectangles, long engineHandle) {
        super(context);

        mRectangles = rectangles;
        mIsPressedPerRectangle = new boolean[rectangles.size()];
        mPaint = new Paint();
        mLocationsOfFingers = new SparseArray<PointF>();
        mEngineHandle = engineHandle;
    }

    private int getIndexFromLocation(PointF pointF) {
        for (int i = 0; i < mRectangles.size(); i++) {
            if (pointF.x > mRectangles.get(i).left &&
                    pointF.x < mRectangles.get(i).right &&
                    pointF.y > mRectangles.get(i).top &&
                    pointF.y < mRectangles.get(i).bottom) {
                return i;
            }
        }
        return -1;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        for (int i = 0; i < mRectangles.size(); i++) {
            mPaint.setStyle(Paint.Style.FILL);
            if (mIsPressedPerRectangle[i]) {
                mPaint.setColor(Color.MAGENTA);
            } else {
                mPaint.setColor(Color.BLACK);
            }
            canvas.drawRect(mRectangles.get(i), mPaint);

            // border
            mPaint.setStyle(Paint.Style.STROKE);
            mPaint.setStrokeWidth(10);
            mPaint.setColor(Color.WHITE);
            canvas.drawRect(mRectangles.get(i), mPaint);

            // Call c++ native code to set each index add/remove sound
            tap(mEngineHandle, mIsPressedPerRectangle[i], i);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int pointerIndex = event.getActionIndex();
        int pointerId = event.getPointerId(pointerIndex);
        int maskedAction = event.getActionMasked();

        switch (maskedAction) {
            // Move each point from it's current point to the new point.
            case MotionEvent.ACTION_MOVE: {
                for (int size = event.getPointerCount(), i = 0; i < size; i++) {
                    PointF point = mLocationsOfFingers.get(event.getPointerId(i));
                    if (point != null) {
                        int curIndex = getIndexFromLocation(point);
                        if (curIndex != -1) {
                            mIsPressedPerRectangle[curIndex] = false;
                        }
                        point.x = event.getX(i);
                        point.y = event.getY(i);

                        curIndex = getIndexFromLocation(point);
                        if (curIndex != -1) {
                            mIsPressedPerRectangle[curIndex] = true;
                        }
                    }
                }
                break;
            }
            // Add a new point when a location is pressed
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                PointF f = new PointF();
                f.x = event.getX(pointerIndex);
                f.y = event.getY(pointerIndex);
                mLocationsOfFingers.put(pointerId, f);
                int curIndex = getIndexFromLocation(f);
                if (curIndex != -1) {
                    mIsPressedPerRectangle[curIndex] = true;
                }
                break;
            }
            // Remove a point when a location lifted or when there is an error
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                int curIndex = getIndexFromLocation(mLocationsOfFingers.get(event.getPointerId(pointerIndex)));
                if (curIndex != -1) {
                    mIsPressedPerRectangle[curIndex] = false;
                }
                mLocationsOfFingers.remove(pointerId);
                break;
            }
        }

        // Calling invalidate() will force onDraw() to be called
        invalidate();

        return true;
    }
}
