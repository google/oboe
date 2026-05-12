/*
 * Copyright (C) 2026 The Android Open Source Project
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

package com.mobileer.oboetester;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import java.util.Locale;

public class FftWaveformView extends WaveformView {

    private Paint mAxisPaint;
    private float mMinDbfs = -100.0f;
    private float mMaxDbfs = 0.0f;

    public FftWaveformView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        mAxisPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mAxisPaint.setColor(Color.GRAY);
        mAxisPaint.setTextSize(30);
        mAxisPaint.setStrokeWidth(2.0f);
    }

    public void setDbfsRange(float minDbfs, float maxDbfs) {
        mMinDbfs = minDbfs;
        mMaxDbfs = maxDbfs;
        invalidate();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float paddingBottom = 60.0f;
        float graphHeight = h - paddingBottom;
        mOffsetY = graphHeight / 2.0f;
        mScaleY = -0.90f * mOffsetY;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (mData == null || mSampleCount == 0) {
            return;
        }

        float midDbfs = (mMinDbfs + mMaxDbfs) / 2.0f;

        // Draw Y axis labels (dBFS) and lines
        canvas.drawText(String.format(Locale.getDefault(), "%.0f dBFS", mMaxDbfs), 10,
                mOffsetY + mScaleY + 30, mAxisPaint);
        canvas.drawText(String.format(Locale.getDefault(), "%.0f dBFS", midDbfs), 10, mOffsetY + 10,
                mAxisPaint);
        canvas.drawText(String.format(Locale.getDefault(), "%.0f dBFS", mMinDbfs), 10,
                mOffsetY - mScaleY - 10, mAxisPaint);

        canvas.drawLine(0, mOffsetY + mScaleY, mCurrentWidth, mOffsetY + mScaleY, mAxisPaint);
        canvas.drawLine(0, mOffsetY, mCurrentWidth, mOffsetY, mAxisPaint);
        canvas.drawLine(0, mOffsetY - mScaleY, mCurrentWidth, mOffsetY - mScaleY, mAxisPaint);
    }

    @Override
    public void setSampleData(float[] samples, int offset, int count) {
        float[] mappedSamples = new float[count];
        for (int i = 0; i < count; i++) {
            mappedSamples[i] = mapDbfsToView(samples[offset + i]);
        }
        super.setSampleData(mappedSamples, 0, count);
    }

    private float mapDbfsToView(float dbfs) {
        float mapped = ((dbfs - mMinDbfs) / (mMaxDbfs - mMinDbfs)) * 2.0f - 1.0f;
        if (mapped < -1.0f) {
            mapped = -1.0f;
        }
        if (mapped > 1.0f) {
            mapped = 1.0f;
        }
        return mapped;
    }
}

