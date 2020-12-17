package com.google.sample.oboe.manualtest;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.text.method.ScrollingMovementMethod;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;

/**
 * Run an automated test from a UI, gather logs,
 * and display a summary.
 */
public  class AutomatedTestRunner extends LinearLayout implements Runnable {

    private Button       mStartButton;
    private Button       mStopButton;
    private Button       mShareButton;
    private TextView     mAutoTextView;
    private TextView     mSingleTestIndex;
    private StringBuffer mFailedSummary;
    private StringBuffer mSummary;
    private int          mTestCount;
    private int          mPassCount;
    private int          mFailCount;
    private TestAudioActivity  mActivity;

    private Thread            mAutoThread;
    private volatile boolean  mThreadEnabled;
    private CachedTextViewLog mCachedTextView;

    public AutomatedTestRunner(Context context) {
        super(context);
        initializeViews(context);
    }

    public AutomatedTestRunner(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public AutomatedTestRunner(Context context,
                               AttributeSet attrs,
                               int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    public TestAudioActivity getActivity() {
        return mActivity;
    }

    public void setActivity(TestAudioActivity activity) {
        this.mActivity = activity;
        mCachedTextView = new CachedTextViewLog(activity, mAutoTextView);
    }

    /**
     * Inflates the views in the layout.
     *
     * @param context
     *           the current context for the view.
     */
    private void initializeViews(Context context) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.auto_test_runner, this);

        mStartButton = (Button) findViewById(R.id.button_start);
        mStartButton.setOnClickListener( new OnClickListener() {
            @Override
            public void onClick(View v) {
                startTest();
            }
        });

        mStopButton = (Button) findViewById(R.id.button_stop);
        mStopButton.setOnClickListener( new OnClickListener() {
            @Override
            public void onClick(View v) {
                stopTest();
            }
        });

        mShareButton = (Button) findViewById(R.id.button_share);
        mShareButton.setOnClickListener( new OnClickListener() {
            @Override
            public void onClick(View v) {
                shareResult();
                mShareButton.setEnabled(true);
            }
        });
        mShareButton.setEnabled(false);

        mSingleTestIndex = (TextView) findViewById(R.id.single_test_index);

        mAutoTextView = (TextView) findViewById(R.id.text_log);
        mAutoTextView.setMovementMethod(new ScrollingMovementMethod());
    }

    private void updateStartStopButtons(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
    }

    public int getTestCount() {
        return mTestCount;
    }

    public boolean isThreadEnabled() {
        return mThreadEnabled;
    }

    public void appendFailedSummary(String text) {
        mFailedSummary.append(text);
    }
    public void appendSummary(String text) {
        mSummary.append(text);
    }

    public void incrementFailCount() {
        mFailCount++;
    }
    public void incrementPassCount() {
        mPassCount++;
    }
    public void incrementTestCount() {
        mTestCount++;
    }

    // Write to scrollable TextView
    public void log(final String text) {
        if (text == null) return;
        Log.d(TestAudioActivity.TAG, "LOG - " + text);
        mCachedTextView.append(text + "\n");
    }

    // Flush any logs that are stuck in the cache.
    public void flushLog() {
        mCachedTextView.flush();
    }

    private void logClear() {
        mCachedTextView.clear();
    }

    private void startAutoThread() {
        mThreadEnabled = true;
        mAutoThread = new Thread(this);
        mAutoThread.start();
    }

    private void stopAutoThread() {
        try {
            if (mAutoThread != null) {
                log("Disable background test thread.");
                new RuntimeException("Disable background test thread.").printStackTrace();
                mThreadEnabled = false;
                mAutoThread.interrupt();
                mAutoThread.join(100);
                mAutoThread = null;
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void updateTestIndex() {
        CharSequence chars = mSingleTestIndex.getText();
        String text = chars.toString();
        int testIndex = -1;
        String trimmed = chars.toString().trim();
        if (trimmed.length() > 0) {
            try {
                testIndex = Integer.parseInt(text);
            } catch (NumberFormatException e) {
                mActivity.showErrorToast("Badly formated callback size: " + text);
                mSingleTestIndex.setText("");
            }
        }
        mActivity.setSingleTestIndex(testIndex);
    }

    protected void startTest() {
        updateTestIndex();
        updateStartStopButtons(true);
        startAutoThread();
    }

    public void stopTest() {
        stopAutoThread();
    }

    // Only call from UI thread.
    public void onTestFinished() {
        updateStartStopButtons(false);
        mShareButton.setEnabled(true);
    }

    public static String getTimestampString() {
        DateFormat df = new SimpleDateFormat("yyyyMMdd-HHmmss");
        Date now = Calendar.getInstance().getTime();
        return df.format(now);
    }

    // Share text from log via GMail, Drive or other method.
    public void shareResult() {
        Intent sharingIntent = new Intent(android.content.Intent.ACTION_SEND);
        sharingIntent.setType("text/plain");

        String subjectText = "OboeTester-" + mActivity.getTestName()
                + "-" + Build.MANUFACTURER
                + "-" + Build.MODEL
                + "-" + getTimestampString();
        subjectText = subjectText.replace(' ', '-');
        sharingIntent.putExtra(android.content.Intent.EXTRA_SUBJECT, subjectText);

        String shareBody = mAutoTextView.getText().toString();
        sharingIntent.putExtra(android.content.Intent.EXTRA_TEXT, shareBody);

        mActivity.startActivity(Intent.createChooser(sharingIntent, "Share using:"));
    }

    @Override
    public void run() {
        logClear();
        log("=== STARTED at " + new Date());
        log(mActivity.getTestName());
        log(MainActivity.getVersiontext());
        log(Build.MANUFACTURER + ", " + Build.MODEL + ", " + Build.PRODUCT);
        log(Build.DISPLAY);
        mFailedSummary = new StringBuffer();
        mSummary = new StringBuffer();
        appendFailedSummary("Summary\n");
        mTestCount = 0;
        mPassCount = 0;
        mFailCount = 0;
        try {
            mActivity.runTest();
            log("Tests finished without exception.");
        } catch(Exception e) {
            log("EXCEPTION: " + e.getMessage());
        } finally {
            mActivity.stopTest();
            if (mThreadEnabled) {
                log("\n==== SUMMARY ========");
                log(mSummary.toString());
                if (mFailCount > 0) {
                    log("These tests FAILED:");
                    log(mFailedSummary.toString());
                    log("------------");
                } else if (mPassCount > 0) {
                    log("All " + mPassCount + " tests PASSED.");
                } else {
                    log("No tests were run!");
                }
                int skipped = mTestCount - (mPassCount + mFailCount);
                log(mPassCount + " passed. "
                        + mFailCount + " failed. "
                        + skipped + " skipped. ");
                log("== FINISHED at " + new Date());
            } else {
                log("== TEST STOPPED ==");
            }
            flushLog();
            mActivity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    onTestFinished();
                }
            });
        }
    }

}
