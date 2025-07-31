package com.mobileer.oboetester;

import android.app.Activity;
import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * A helper class to manage volume control buttons.
 * This class finds the volume buttons and a spinner in the provided view,
 * populates the spinner with audio stream types, and sets up OnClickListeners
 * to use the AudioManager to adjust the volume for the selected stream.
 */
public class VolumeControl {

    private Activity mActivity;
    private View mRootView;
    private AudioManager mAudioManager;
    private int mCurrentStreamType = AudioManager.STREAM_MUSIC;
    private final Map<String, Integer> mStreamTypes = new LinkedHashMap<>();

    /**
     * Constructor for the VolumeControl class.
     *
     * @param activity The activity that hosts the buttons.
     * @param rootView The root view of the layout where the buttons are defined.
     * This is typically the activity's content view.
     */
    public VolumeControl(Activity activity, View rootView) {
        this.mActivity = activity;
        this.mRootView = rootView;
        this.mAudioManager = (AudioManager) activity.getSystemService(Context.AUDIO_SERVICE);

        // Populate the map of stream types
        mStreamTypes.put("Music", AudioManager.STREAM_MUSIC);
        mStreamTypes.put("Ring", AudioManager.STREAM_RING);
        mStreamTypes.put("Alarm", AudioManager.STREAM_ALARM);
        mStreamTypes.put("Notification", AudioManager.STREAM_NOTIFICATION);
        mStreamTypes.put("System", AudioManager.STREAM_SYSTEM);
        mStreamTypes.put("Voice Call", AudioManager.STREAM_VOICE_CALL);
        mStreamTypes.put("DTMF", AudioManager.STREAM_DTMF);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mStreamTypes.put("Accessibility", AudioManager.STREAM_ACCESSIBILITY);
        }

        setupControls();
    }

    /**
     * Finds the volume controls within the layout, populates the spinner,
     * and attaches necessary listeners.
     */
    private void setupControls() {
        Button volumeUpButton = mRootView.findViewById(R.id.volume_up_button);
        Button volumeDownButton = mRootView.findViewById(R.id.volume_down_button);
        Spinner streamTypeSpinner = mRootView.findViewById(R.id.stream_type_spinner);

        // Setup Spinner
        if (streamTypeSpinner != null) {
            ArrayAdapter<String> adapter = new ArrayAdapter<>(
                    mActivity,
                    android.R.layout.simple_spinner_item,
                    new ArrayList<>(mStreamTypes.keySet())
            );
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            streamTypeSpinner.setAdapter(adapter);

            streamTypeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    String selectedStreamName = (String) parent.getItemAtPosition(position);
                    Integer streamType = mStreamTypes.get(selectedStreamName);
                    if (streamType != null) {
                        mCurrentStreamType = streamType;
                    }
                }

                @Override
                public void onNothingSelected(AdapterView<?> parent) {
                    // Do nothing
                }
            });
        }


        // Setup Volume Up Button
        if (volumeUpButton != null) {
            volumeUpButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mAudioManager != null) {
                        mAudioManager.adjustStreamVolume(mCurrentStreamType,
                                AudioManager.ADJUST_RAISE,
                                AudioManager.FLAG_SHOW_UI);
                    }
                }
            });
        }

        // Setup Volume Down Button
        if (volumeDownButton != null) {
            volumeDownButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mAudioManager != null) {
                        mAudioManager.adjustStreamVolume(mCurrentStreamType,
                                AudioManager.ADJUST_LOWER,
                                AudioManager.FLAG_SHOW_UI);
                    }
                }
            });
        }
    }
}
