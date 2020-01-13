/*
 * Copyright 2019 The Android Open Source Project
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
package com.google.oboe.sample.drumthumper

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View


class TriggerPad: View {
    val TAG = "DrumPad";

    private val mDrawRect = RectF()
    private val mPaint = Paint()

    private val mUpColor = Color.LTGRAY
    private val mDownColor = Color.DKGRAY
    private var mIsDown = false;

    private var mText = "DrumPad";
    private var mTextSizeSp = 28.0f

    private val mTextColor = Color.BLACK

    private val DISPLAY_MASK        = 0x00000003
    private val DISPLAY_RECT        = 0x00000000
    private val DISPLAY_CIRCLE      = 0x00000001
    private val DISPLAY_ROUNDRECT   = 0x00000002

    private var mDisplayFlags = DISPLAY_ROUNDRECT

    interface DrumPadTriggerListener {
        fun triggerDown(pad: TriggerPad)
        fun triggerUp(pad: TriggerPad)
    }

    var mListeners = ArrayList<DrumPadTriggerListener>()

    constructor(context: Context) : super(context) {
    }

    constructor(context: Context, attrs: AttributeSet) : super(context, attrs) {
        extractAttributes(context, attrs)
    }

    constructor(context: Context, attrs: AttributeSet, defStyle: Int): super(context, attrs, defStyle) {
        extractAttributes(context, attrs)
    }

    //
    // Attributes
    //
    private fun extractAttributes(context: Context, attrs: AttributeSet) {
        val xmlns = "http://schemas.android.com/apk/res/android"
        val textVal = attrs.getAttributeValue(xmlns, "text");
        if (textVal != null) {
            mText = textVal;
        }
    }

    //
    // Layout Routines
    //
    private fun calcTextSizeInPixels() : Float {
        val scaledSizeInPixels: Float = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP,
                mTextSizeSp, resources.displayMetrics)

        return scaledSizeInPixels;
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        var padLeft = getPaddingLeft()
        var padRight = getPaddingRight()
        var padTop = getPaddingTop()
        var padBottom = getPaddingBottom()

        mDrawRect.set(padLeft.toFloat(),
                padTop.toFloat(),
                w - padRight.toFloat(),
                h - padBottom.toFloat())

        // mTextSize = mDrawRect.bottom / 4.0f
    }

    override fun onMeasure (widthMeasureSpec: Int, heightMeasureSpec: Int): Unit {
        var widthMode = MeasureSpec.getMode(widthMeasureSpec);
        var width = MeasureSpec.getSize(widthMeasureSpec);

//        var padLeft = getPaddingLeft()
//        var padRight = getPaddingRight()
        var padTop = getPaddingTop()
        var padBottom = getPaddingBottom()


        var heightMode = MeasureSpec.getMode(heightMeasureSpec);
        var height = MeasureSpec.getSize(heightMeasureSpec);

        var textSizePixels = calcTextSizeInPixels()
        when (heightMode) {
            MeasureSpec.AT_MOST -> run {
                // mText = "AT_MOST"
                var newHeight = (textSizePixels.toInt() * 2) + padTop + padBottom
                height = minOf(height, newHeight) }

            MeasureSpec.EXACTLY -> run {
                /*mText = "EXACTLY"*/ }

            MeasureSpec.UNSPECIFIED -> run {
                // mText = "UNSPECIFIED"
                height = textSizePixels.toInt() }
        }

        setMeasuredDimension(width, height);
    }

    //
    // Drawing Routines
    //
    protected override fun onDraw(canvas: Canvas): Unit {
        // Face
        if (mIsDown) {
            mPaint.setColor(mDownColor)
        } else {
            mPaint.setColor(mUpColor)
        }

        when (mDisplayFlags and DISPLAY_MASK) {
            DISPLAY_RECT -> canvas.drawRect(mDrawRect, mPaint)

            DISPLAY_CIRCLE -> run {
                var midX = mDrawRect.left + mDrawRect.width() / 2.0f
                var midY = mDrawRect.top + mDrawRect.height() / 2.0f
                var radius = minOf(mDrawRect.height() / 2.0f, mDrawRect.width() / 2.0f)
                canvas.drawCircle(midX, midY, radius - 5.0f, mPaint)
            }

            DISPLAY_ROUNDRECT -> run {
                var rad = minOf(mDrawRect.width() / 8.0f, mDrawRect.height() / 8.0f)
                canvas.drawRoundRect(mDrawRect, rad, rad, mPaint)
            }
        }

        // Text
        val midX = mDrawRect.width() / 2
        mPaint.setTextSize(calcTextSizeInPixels())
        val textWidth = mPaint.measureText(mText)
        mPaint.setColor(mTextColor)
        var textSizePixels = calcTextSizeInPixels()
        canvas.drawText(mText, mDrawRect.left + midX - textWidth / 2,
                mDrawRect.bottom/2 + textSizePixels/2, mPaint)

    }

    //
    // Input Routines
    //
    override fun onTouchEvent(event: MotionEvent): Boolean {
        val action = event.getActionMasked()
        when (action) {
            MotionEvent.ACTION_DOWN -> {
                mIsDown = true;
                triggerDown()
                invalidate()
                return true
            }

            MotionEvent.ACTION_UP -> {
                mIsDown = false;
                triggerUp()
                invalidate()
                return true
            }
        }

        return false
    }

    //
    // Event Listeners
    //
    fun addListener(listener: DrumPadTriggerListener) {
        mListeners.add(listener)
    }

    fun triggerDown() {
        for( listener in mListeners) {
            listener.triggerDown(this)
        }
    }

    fun triggerUp() {
        for( listener in mListeners) {
            listener.triggerUp(this)
        }
    }
}
