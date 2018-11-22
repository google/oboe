/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.sample.oboe.manualtest;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

public class VolumeBarView extends View {

    private Paint mBarPaint;
    private int mCurrentWidth;
    private int mCurrentHeight;
    private Paint mBackgroundPaint;
    private float mVolume;

    public VolumeBarView(Context context, AttributeSet attrs) {
        super(context, attrs);
//        TypedArray a = context.getTheme().obtainStyledAttributes(attrs,
//                R.styleable.VolumeBarView, 0, 0);
        init();
    }

    private void init() {
        mBarPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBarPaint.setColor(Color.RED);
        mBarPaint.setStyle(Paint.Style.FILL);

        mBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBackgroundPaint.setColor(Color.LTGRAY);
        mBackgroundPaint.setStyle(Paint.Style.FILL);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mCurrentWidth = w;
        mCurrentHeight = h;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawRect(0.0f, 0.0f, mCurrentWidth,
                mCurrentHeight, mBackgroundPaint);
        float scaledVolume = mVolume * mCurrentWidth;
        canvas.drawRect(0.0f, 0.0f, scaledVolume,
                mCurrentHeight, mBarPaint);
    }

    /**
     * Set volume between 0.0 and 1.0
     */
    public void setVolume(float volume) {
        mVolume = volume;
        postInvalidate();
    }

}
