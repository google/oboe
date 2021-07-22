package com.google.oboe.samples.soundboard;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PointF;
import android.graphics.Rect;
import android.provider.ContactsContract;
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
    private NoteListener mNoteListener;

    public MusicTileView(Context context, ArrayList<Rect> rectangles, NoteListener noteListener) {
        super(context);

        mRectangles = rectangles;
        mIsPressedPerRectangle = new boolean[rectangles.size()];
        mPaint = new Paint();
        mLocationsOfFingers = new SparseArray<PointF>();
        mNoteListener = noteListener;
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
                mPaint.setColor(Color.rgb(128, 0, 0));
            } else {
                mPaint.setColor(Color.BLACK);
            }
            canvas.drawRect(mRectangles.get(i), mPaint);

            // border
            mPaint.setStyle(Paint.Style.STROKE);
            mPaint.setStrokeWidth(10);
            mPaint.setColor(Color.WHITE);
            canvas.drawRect(mRectangles.get(i), mPaint);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int pointerIndex = event.getActionIndex();
        int pointerId = event.getPointerId(pointerIndex);
        int maskedAction = event.getActionMasked();

        boolean didImageChange = false;
        switch (maskedAction) {
            // Move each point from it's current point to the new point.
            case MotionEvent.ACTION_MOVE: {
                int[] notesChangedBy = new int[mRectangles.size()];
                for (int size = event.getPointerCount(), i = 0; i < size; i++) {
                    PointF point = mLocationsOfFingers.get(event.getPointerId(i));
                    if (point != null) {
                        int prevIndex = getIndexFromLocation(point);
                        point.x = event.getX(i);
                        point.y = event.getY(i);
                        int newIndex = getIndexFromLocation(point);

                        if (newIndex != prevIndex) {
                            if (prevIndex != -1) {
                                notesChangedBy[prevIndex]--;
                            }
                            if (newIndex != -1) {
                                notesChangedBy[newIndex]++;
                            }
                        }
                    }
                }

                // Now go through the rectangles to see if they have changed
                for (int i = 0; i < mRectangles.size(); i++) {
                    if (notesChangedBy[i] > 0) {
                        mIsPressedPerRectangle[i] = true;
                        mNoteListener.onTileOn(i);
                        didImageChange = true;
                    } else if (notesChangedBy[i] < 0) {
                        mIsPressedPerRectangle[i] = false;
                        mNoteListener.onTileOff(i);
                        didImageChange = true;
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
                    mNoteListener.onTileOn(curIndex);
                    didImageChange = true;
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
                    mNoteListener.onTileOff(curIndex);
                    didImageChange = true;
                }
                mLocationsOfFingers.remove(pointerId);
                break;
            }
        }

        // Calling invalidate() will force onDraw() to be called
        if (didImageChange) {
            invalidate();
        }

        return true;
    }
}
