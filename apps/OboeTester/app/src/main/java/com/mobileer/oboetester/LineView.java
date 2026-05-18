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

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

public class LineView extends View {

    public enum DrawMode {
        LINE,
        BAR
    }

    private static class LineData {

        int id;
        float[] xValues;
        float[] yValues;
        int color;
        Paint paint;
        DrawMode drawMode;

        LineData(int id, float[] xValues, float[] yValues, int color, DrawMode drawMode) {
            this.id = id;
            this.xValues = xValues;
            this.yValues = yValues;
            this.color = color;
            this.drawMode = drawMode != null ? drawMode : DrawMode.LINE;
            this.paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            this.paint.setColor(color);
            this.paint.setStrokeWidth(3.0f);
            this.paint.setStyle(Paint.Style.STROKE);
        }
    }

    private final Map<Integer, LineData> mLines = new HashMap<>();
    private int mNextLineId = 1;

    private float[] mGridLinesX;
    private float[] mGridLinesY;

    private Paint mGridPaintX;
    private Paint mGridTextPaintX;

    private Paint mGridPaintY;
    private Paint mGridTextPaintY;

    private String mUnitX = "";
    private String mUnitY = "";

    private float mMinX = 0.0f;
    private float mMaxX = 1.0f;
    private float mMinY = -100.0f;
    private float mMaxY = 0.0f;
    private boolean mLogScaleX = false;

    public LineView(Context context) {
        super(context);
        init();
    }

    public LineView(Context context, String unitX, String unitY) {
        super(context);
        mUnitX = unitX != null ? unitX : "";
        mUnitY = unitY != null ? unitY : "";
        init();
    }

    public LineView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        setBackgroundResource(R.color.waveform_background);

        mGridPaintX = new Paint(Paint.ANTI_ALIAS_FLAG);
        mGridPaintX.setColor(Color.GRAY);
        mGridPaintX.setStrokeWidth(2.0f);

        mGridTextPaintX = new Paint(Paint.ANTI_ALIAS_FLAG);
        mGridTextPaintX.setColor(Color.GRAY);
        mGridTextPaintX.setTextSize(30.0f);

        mGridPaintY = new Paint(Paint.ANTI_ALIAS_FLAG);
        mGridPaintY.setColor(Color.GRAY);
        mGridPaintY.setStrokeWidth(2.0f);

        mGridTextPaintY = new Paint(Paint.ANTI_ALIAS_FLAG);
        mGridTextPaintY.setColor(Color.GRAY);
        mGridTextPaintY.setTextSize(30.0f);
    }

    public void setUnits(String unitX, String unitY) {
        mUnitX = unitX != null ? unitX : "";
        mUnitY = unitY != null ? unitY : "";
        postInvalidate();
    }

    public void setXRange(float minX, float maxX) {
        mMinX = minX;
        mMaxX = maxX;
        postInvalidate();
    }

    public void setLogScaleX(boolean logScale) {
        mLogScaleX = logScale;
        postInvalidate();
    }

    public void setYRange(float minY, float maxY) {
        mMinY = minY;
        mMaxY = maxY;
        postInvalidate();
    }

    public void setGridLinesX(float[] xValues) {
        mGridLinesX = xValues;
        postInvalidate();
    }

    public void setGridLinesY(float[] yValues) {
        mGridLinesY = yValues;
        postInvalidate();
    }

    public int addLine(float[] xValues, float[] yValues, int color) {
        return addLine(xValues, yValues, color, DrawMode.LINE);
    }

    public int addLine(float[] xValues, float[] yValues, int color, DrawMode drawMode) {
        int id = mNextLineId++;
        LineData line = new LineData(id, xValues, yValues, color, drawMode);
        mLines.put(id, line);
        postInvalidate();
        return id;
    }

    public void updateLine(int id, float[] xValues, float[] yValues) {
        LineData line = mLines.get(id);
        if (line != null) {
            line.xValues = xValues;
            line.yValues = yValues;
            postInvalidate();
        }
    }

    public void removeLine(int id) {
        if (mLines.remove(id) != null) {
            postInvalidate();
        }
    }

    public void removeAllLines() {
        mLines.clear();
        postInvalidate();
    }

    private float mapYToView(float y, float scaleY, float offsetY) {
        float mapped = ((y - mMinY) / (mMaxY - mMinY)) * 2.0f - 1.0f;
        if (mapped < -1.0f) {
            mapped = -1.0f;
        }
        if (mapped > 1.0f) {
            mapped = 1.0f;
        }
        return (mapped * scaleY) + offsetY;
    }

    private float mapXToView(float xVal, float graphWidth, float paddingLeft) {
        if (mLogScaleX) {
            if (mMinX <= 0 || mMaxX <= mMinX) {
                return paddingLeft;
            }
            double logMin = Math.log(mMinX);
            double logMax = Math.log(mMaxX);
            double logX = Math.log(xVal);
            return (float) (((logX - logMin) / (logMax - logMin)) * graphWidth) + paddingLeft;
        } else {
            float rangeX = mMaxX - mMinX;
            if (rangeX <= 0) {
                return paddingLeft;
            }
            return ((xVal - mMinX) / rangeX) * graphWidth + paddingLeft;
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        int width = getWidth();
        int height = getHeight();

        if (width <= 0 || height <= 0) {
            return;
        }

        float rangeX = mMaxX - mMinX;
        if (rangeX <= 0 || (mMaxY - mMinY) == 0) {
            return;
        }

        float paddingBottom = 60.0f;
        float paddingTop = 60.0f;
        float paddingLeft = 100.0f;
        float paddingRight = 100.0f;
        float graphWidth = width - paddingLeft - paddingRight;
        float graphHeight = height - paddingBottom - paddingTop;
        float offsetY = paddingTop + (graphHeight / 2.0f);
        float scaleY = -0.90f * (graphHeight / 2.0f);

        // Draw lines
        for (LineData line : mLines.values()) {
            if (line.xValues == null || line.yValues == null) {
                continue;
            }
            int count = Math.min(line.xValues.length, line.yValues.length);
            if (count < 1) {
                continue;
            }

            if (line.drawMode == DrawMode.BAR) {
                float yBottom = mapYToView(mMinY, scaleY, offsetY);
                for (int i = 0; i < count; i++) {
                    float xVal = line.xValues[i];
                    if (xVal < mMinX || xVal > mMaxX) {
                        continue;
                    }
                    float x = mapXToView(xVal, graphWidth, paddingLeft);
                    float y = mapYToView(line.yValues[i], scaleY, offsetY);
                    canvas.drawLine(x, yBottom, x, y, line.paint);
                }
            } else {
                float x0 = -1;
                float y0 = -1;
                for (int i = 0; i < count; i++) {
                    float xVal = line.xValues[i];
                    if (xVal < mMinX || xVal > mMaxX) {
                        continue;
                    }
                    float x1 = mapXToView(xVal, graphWidth, paddingLeft);
                    float y1 = mapYToView(line.yValues[i], scaleY, offsetY);
                    if (x0 != -1) {
                        canvas.drawLine(x0, y0, x1, y1, line.paint);
                    }
                    x0 = x1;
                    y0 = y1;
                }
            }
        }

        // Draw Y grid lines
        if (mGridLinesY != null) {
            for (float yVal : mGridLinesY) {
                float yPos = mapYToView(yVal, scaleY, offsetY);
                canvas.drawLine(paddingLeft, yPos, width - paddingRight, yPos, mGridPaintY);
                String label = String.format(Locale.getDefault(), "%.0f", yVal);
                mGridTextPaintY.setTextAlign(Paint.Align.LEFT);
                canvas.drawText(label, 10, yPos - 10, mGridTextPaintY);
            }
            if (!mUnitY.isEmpty()) {
                mGridTextPaintY.setTextAlign(Paint.Align.LEFT);
                canvas.drawText(mUnitY, 10, 30, mGridTextPaintY);
            }
        }

        // Draw X grid lines
        if (mGridLinesX != null) {
            for (float xVal : mGridLinesX) {
                if (xVal < mMinX || xVal > mMaxX) {
                    continue;
                }
                float x = mapXToView(xVal, graphWidth, paddingLeft);
                canvas.drawLine(x, paddingTop, x, height - paddingBottom, mGridPaintX);
                String label;
                if (Math.abs(xVal) >= 1000 && xVal % 1000 == 0) {
                    label = String.format(Locale.getDefault(), "%.0fk", xVal / 1000);
                } else {
                    label = String.format(Locale.getDefault(), "%.0f", xVal);
                }
                mGridTextPaintX.setTextAlign(Paint.Align.LEFT);
                canvas.drawText(label, x + 5, height - 15, mGridTextPaintX);
            }
            if (!mUnitX.isEmpty()) {
                mGridTextPaintX.setTextAlign(Paint.Align.RIGHT);
                canvas.drawText(mUnitX, width - 5, height - 15, mGridTextPaintX);
            }
        }
    }
}
