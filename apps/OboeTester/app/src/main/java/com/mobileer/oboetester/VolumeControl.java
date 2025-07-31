package com.mobileer.oboetester;

import android.app.Activity;
import android.content.Context;
import android.media.AudioManager;
import android.view.View;
import android.widget.Button;

/**
 * A helper class to manage volume control buttons.
 * This class finds the volume buttons in the provided view and sets up
 * OnClickListeners to use the AudioManager to adjust the volume.
 */
public class VolumeControl {

    private Activity mActivity;
    private View mRootView;
    private AudioManager mAudioManager;

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
        setupVolumeButtons();
    }

    /**
     * Finds the volume buttons within the layout and attaches click listeners.
     */
    private void setupVolumeButtons() {
        // It's assumed that the layout containing these buttons has been inflated
        // and the IDs are available within the rootView.
        // The IDs R.id.volume_up_button and R.id.volume_down_button must be available
        // in your project's R file.
        Button volumeUpButton = mRootView.findViewById(R.id.volume_up_button);
        Button volumeDownButton = mRootView.findViewById(R.id.volume_down_button);

        if (volumeUpButton != null) {
            volumeUpButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mAudioManager != null) {
                        // Adjust the volume up for the music stream and show the default UI
                        mAudioManager.adjustStreamVolume(AudioManager.STREAM_MUSIC,
                                AudioManager.ADJUST_RAISE,
                                AudioManager.FLAG_SHOW_UI);
                    }
                }
            });
        }

        if (volumeDownButton != null) {
            volumeDownButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mAudioManager != null) {
                        // Adjust the volume down for the music stream and show the default UI
                        mAudioManager.adjustStreamVolume(AudioManager.STREAM_MUSIC,
                                AudioManager.ADJUST_LOWER,
                                AudioManager.FLAG_SHOW_UI);
                    }
                }
            });
        }
    }
}
