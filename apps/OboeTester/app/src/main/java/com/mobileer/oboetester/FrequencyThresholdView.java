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
import android.view.View;
import java.util.Locale;

public class FrequencyThresholdView extends View {
    private float[] mFrequencies;
    private float[] mData;
    private float mMaxFrequency = 0.0f;
    private float mAverageMagnitude = 0.0f;
    private boolean mShowAverageMagnitude = false;

    private Paint mPaint;
    private Paint mAxisPaint;

    public FrequencyThresholdView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mPaint.setColor(Color.RED);
        mPaint.setStrokeWidth(3.0f);
        mPaint.setStyle(Paint.Style.STROKE);

        mAxisPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mAxisPaint.setColor(Color.GRAY);
        mAxisPaint.setTextSize(30);
        mAxisPaint.setStrokeWidth(2.0f);
    }

    public void updateTheme(int waveColor, int backgroundColor, int cursorColor) {
        mPaint.setColor(waveColor);
        invalidate();
    }

    public void setMaxFrequency(float maxFrequency) {
        mMaxFrequency = maxFrequency;
        invalidate();
    }

    public void setFrequencies(float[] frequencies) {
        mFrequencies = frequencies;
        invalidate();
    }

    public void setSampleData(float[] data) {
        mData = data;
        invalidate();
    }

    public void setAverageMagnitude(float mappedValue) {
        mAverageMagnitude = mappedValue;
        mShowAverageMagnitude = true;
        invalidate();
    }

    public void clearAverageMagnitude() {
        mShowAverageMagnitude = false;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (mData == null || mFrequencies == null || mData.length == 0 || mFrequencies.length == 0) {
            return;
        }

        int width = getWidth();
        int height = getHeight();
        float paddingBottom = 60.0f;
        float graphHeight = height - paddingBottom;
        float offsetY = graphHeight / 2.0f;
        float scaleY = -0.90f * offsetY; // Scale down to match FftWaveformView

        float maxFreq = mMaxFrequency > 0 ? mMaxFrequency : mFrequencies[mFrequencies.length - 1];
        if (maxFreq <= 0) return;

        if (mShowAverageMagnitude) {
            float yAvg = (mAverageMagnitude * scaleY) + offsetY;
            Paint avgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            avgPaint.setColor(android.graphics.Color.parseColor("#FF2196F3"));
            avgPaint.setStrokeWidth(3.0f);
            canvas.drawLine(0, yAvg, width, yAvg, avgPaint);
        }

        for (int i = 0; i < mFrequencies.length; i++) {
            float f = mFrequencies[i];
            float x = (f / maxFreq) * width;
            String label = String.format(Locale.getDefault(), "%.0f Hz", f);
            canvas.drawText(label, x + 5, height - 15, mAxisPaint);
            canvas.drawLine(x, 0, x, graphHeight, mAxisPaint);
        }

        float x0 = (mFrequencies[0] / maxFreq) * width;
        float y0 = (mData[0] * scaleY) + offsetY;

        for (int i = 1; i < mData.length; i++) {
            if (i >= mFrequencies.length) break;
            float x1 = (mFrequencies[i] / maxFreq) * width;
            float y1 = (mData[i] * scaleY) + offsetY;
            canvas.drawLine(x0, y0, x1, y1, mPaint);
            x0 = x1;
            y0 = y1;
        }
    }
}
