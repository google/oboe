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


class DrumPad: View {
    val TAG = "DrumPad";

    private val mDrawRect = RectF()
    private val mPaint = Paint()

    private val mUpColor = Color.LTGRAY
    private val mDownColor = Color.DKGRAY
    private var mIsDown = false;

    private var mText = "DrumPad";
    private var mTextSizeSp = 28.0f

    private val mTextColor = Color.BLACK

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
//        var set: IntArray = intArrayOf(android.R.attr.text)
//        var attrVal = set.get(0)
//
//        var styleAttrs = context.obtainStyledAttributes(set)
//        var numStyes = styleAttrs.getIndexCount()
//
//        var numAtts = attrs.attributeCount
//        var attrName =  ""
//        var textIndex = -1
//        for(i in 0..numAtts-1) {
//            attrName = attrs.getAttributeName(i)
//            if (attrName == "text") {
//                textIndex = i
//                break
//            }
//        }
//
//        // var styledCound = styleAttrs.indexCount
//
//        if (textIndex != -1) {
//            var chars = attrs.getAttributeValue(3);
//            mText = chars.toString()
//        }
//
        var xmlns = "http://schemas.android.com/apk/res/android"
        var textVal = attrs.getAttributeValue(xmlns, "text");
        if (textVal != null) {
            mText = textVal;
        }
//
//        var text = styleAttrs.getString(0)
//        mText = text.toString()
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
        canvas.drawRect(mDrawRect, mPaint)

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
                trigger()
                mIsDown = true;
                invalidate()
                return true
            }

            MotionEvent.ACTION_UP -> {
                mIsDown = false;
                invalidate()
                return true
            }
        }

        return false
    }

    //
    // Event Listeners
    //
    interface DrumPadTriggerListener {
        fun trigger(pad: DrumPad)
    }

    var mListeners = ArrayList<DrumPadTriggerListener>()

    fun addListener(listener: DrumPadTriggerListener) {
        mListeners.add(listener)
    }

    fun trigger() {
        for( listener in mListeners) {
            listener.trigger(this)
        }
    }
}
