package com.mobileer.oboetester;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.widget.Button;

/**
 * A helper class to manage volume control buttons.
 * This class finds the volume buttons in the provided view and sets up
 * OnClickListeners to simulate hardware volume key presses.
 */
public class VolumeControl {

    private final Activity mActivity;
    private final View mRootView;

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
                    sendKeyEvent(KeyEvent.KEYCODE_VOLUME_UP);
                }
            });
        }

        if (volumeDownButton != null) {
            volumeDownButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    sendKeyEvent(KeyEvent.KEYCODE_VOLUME_DOWN);
                }
            });
        }
    }

    /**
     * Simulates a key press event.
     * This creates a new InputConnection on the activity's current focus
     * and sends the specified key event.
     *
     * @param keyCode The key code to send (e.g., KeyEvent.KEYCODE_VOLUME_UP).
     */
    private void sendKeyEvent(int keyCode) {
        View decorView = mActivity.getWindow().getDecorView();
        BaseInputConnection inputConnection = new BaseInputConnection(decorView, true);
        inputConnection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
        inputConnection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keyCode));
    }
}
