package com.mobileer.oboetester;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import java.io.IOException;

public class TestMultiStreamActivity extends TestAudioActivity {

    private class MultiStreamTester {
        AudioStreamTester tester;
        StreamConfigurationView configView;
        View container;

        int mAudioState = AUDIO_STATE_CLOSED;
        Button btnOpen, btnStart, btnPause, btnFlush, btnStop, btnClose;
        VolumeBarView[] mVolumeBars = new VolumeBarView[2];

        MultiStreamTester(View container, boolean isInput) {
            this.container = container;
            this.tester = isInput ? new AudioInputTester() : new AudioOutputTester();
            
            this.configView = container.findViewById(R.id.streamConfiguration);
            this.configView.setOutput(!isInput);
            
            btnOpen = container.findViewById(R.id.button_open);
            btnStart = container.findViewById(R.id.button_start);
            btnPause = container.findViewById(R.id.button_pause);
            btnFlush = container.findViewById(R.id.button_flush);
            btnStop = container.findViewById(R.id.button_stop);
            btnClose = container.findViewById(R.id.button_close);
            
            mVolumeBars[0] = container.findViewById(R.id.volumeBar0);
            mVolumeBars[1] = container.findViewById(R.id.volumeBar1);
            
            btnOpen.setOnClickListener(v -> openStream());
            btnStart.setOnClickListener(v -> startStream());
            btnPause.setOnClickListener(v -> pauseStream());
            btnFlush.setOnClickListener(v -> flushStream());
            btnStop.setOnClickListener(v -> stopStream());
            btnClose.setOnClickListener(v -> closeStream());
            
            updateButtons();
        }
        
        void updateButtons() {
            btnOpen.setBackgroundColor(mAudioState == AUDIO_STATE_OPEN ? COLOR_ACTIVE : COLOR_IDLE);
            btnStart.setBackgroundColor(mAudioState == AUDIO_STATE_STARTED ? COLOR_ACTIVE : COLOR_IDLE);
            btnPause.setBackgroundColor(mAudioState == AUDIO_STATE_PAUSED ? COLOR_ACTIVE : COLOR_IDLE);
            btnFlush.setBackgroundColor(mAudioState == AUDIO_STATE_FLUSHED ? COLOR_ACTIVE : COLOR_IDLE);
            btnStop.setBackgroundColor(mAudioState == AUDIO_STATE_STOPPED ? COLOR_ACTIVE : COLOR_IDLE);
            btnClose.setBackgroundColor(mAudioState == AUDIO_STATE_CLOSED ? COLOR_ACTIVE : COLOR_IDLE);
            configView.setChildrenEnabled(mAudioState == AUDIO_STATE_CLOSED);
        }

        void openStream() {
            try {
                configView.applyToModel(tester.requestedConfiguration);
                tester.open();
                mAudioState = AUDIO_STATE_OPEN;
                configView.updateDisplay(tester.actualConfiguration);
                updateButtons();
            } catch (IOException e) {
                showErrorToast(e.getMessage());
            }
        }

        void startStream() {
            OboeAudioStream stream = (OboeAudioStream) tester.getCurrentAudioStream();
            if (stream != null) {
                stream.start();
                mAudioState = AUDIO_STATE_STARTED;
                updateButtons();
            }
        }

        void pauseStream() {
            OboeAudioStream stream = (OboeAudioStream) tester.getCurrentAudioStream();
            if (stream != null) {
                stream.pause();
                mAudioState = AUDIO_STATE_PAUSED;
                updateButtons();
            }
        }

        void flushStream() {
            OboeAudioStream stream = (OboeAudioStream) tester.getCurrentAudioStream();
            if (stream != null) {
                stream.flush();
                mAudioState = AUDIO_STATE_FLUSHED;
                updateButtons();
            }
        }

        void stopStream() {
            OboeAudioStream stream = (OboeAudioStream) tester.getCurrentAudioStream();
            if (stream != null) {
                stream.stop();
                mAudioState = AUDIO_STATE_STOPPED;
                updateButtons();
            }
        }

        void closeStream() {
            if (tester.getCurrentAudioStream() != null) {
                tester.close();
                mAudioState = AUDIO_STATE_CLOSED;
                configView.updateDisplay(tester.actualConfiguration);
                updateButtons();
            }
        }
    }

    private MultiStreamTester mOut1;
    private MultiStreamTester mOut2;
    private MultiStreamTester mIn1;
    private MultiStreamTester mIn2;
    
    private android.os.Handler mStatusHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private Runnable mStatusRunnable = new Runnable() {
        @Override
        public void run() {
            updateStreamStatus(mOut1);
            updateStreamStatus(mOut2);
            updateStreamStatus(mIn1);
            updateStreamStatus(mIn2);
            mStatusHandler.postDelayed(this, 200);
        }
    };

    private void updateStreamStatus(MultiStreamTester testerContext) {
        if (testerContext == null || testerContext.mAudioState == AUDIO_STATE_CLOSED || testerContext.tester.getCurrentAudioStream() == null) return;
        AudioStreamBase stream = testerContext.tester.getCurrentAudioStream();
        AudioStreamBase.StreamStatus status = stream.getStreamStatus();
        AudioStreamBase.DoubleStatistics latencyStatistics = stream.getLatencyStatistics();
        int errorCode = stream.getLastErrorCallbackResult();
        
        int framesPerBurst = stream.getFramesPerBurst();
        status.framesPerCallback = 0;
        String msg = "";
        msg += "timestamp.latency = " + latencyStatistics.dump() + "\n";
        msg += "lastErrorCallbackResult = " + StreamConfiguration.convertErrorToText(errorCode) + "\n";
        msg += status.dump(framesPerBurst);
        testerContext.configView.setStatusText(msg);
        
        if (testerContext.mAudioState == AUDIO_STATE_STARTED) {
            int numChannels = stream.getChannelCount();
            if (numChannels > 2) numChannels = 2;
            for (int i = 0; i < numChannels; i++) {
                if (testerContext.mVolumeBars[i] != null) {
                    double level = ((OboeAudioStream) stream).getPeakLevel(i);
                    testerContext.mVolumeBars[i].setAmplitude((float) level);
                }
            }
        } else {
            for (int i = 0; i < 2; i++) {
                if (testerContext.mVolumeBars[i] != null) {
                    testerContext.mVolumeBars[i].setAmplitude(0.0f);
                }
            }
        }
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_test_multi_stream);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        mOut1 = new MultiStreamTester(findViewById(R.id.output1), false);
        mOut2 = new MultiStreamTester(findViewById(R.id.output2), false);
        mIn1 = new MultiStreamTester(findViewById(R.id.input1), true);
        mIn2 = new MultiStreamTester(findViewById(R.id.input2), true);

        mCommunicationDeviceView = (CommunicationDeviceView) findViewById(R.id.comm_device_view);
        
        updateEnabledWidgets();
    }

    @Override
    int getActivityType() {
        return ACTIVITY_TEST_MULTI_STREAM;
    }

    @Override
    public void onResume() {
        super.onResume();
        mStatusHandler.post(mStatusRunnable);
    }

    @Override
    public void onPause() {
        super.onPause();
        mStatusHandler.removeCallbacks(mStatusRunnable);
    }

    @Override
    boolean isOutput() {
        return true; // Contains outputs
    }

    @Override
    protected void findAudioCommon() {
        // Initialize this so TestAudioActivity doesn't crash in setConfigViewsEnabled
        mStreamContexts = new java.util.ArrayList<>();
        // Do not call super.findAudioCommon() so TestAudioActivity doesn't hijack the first stream's buttons!
    }

    @Override
    protected void resetConfiguration() {
        super.resetConfiguration();
        mOut1.tester.reset();
        mOut2.tester.reset();
        mIn1.tester.reset();
        mIn2.tester.reset();
    }
}
