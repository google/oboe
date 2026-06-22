package com.mobileer.oboetester;

import android.media.AudioManager;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.util.Log;
import java.util.TimerTask;
import java.util.Timer;

/**
 * Frequency tests require a high volume to generate a clear frequency response from the speaker and
 * microphone; however, the volume level is often left unset. Additionally, peripheral adjustments
 * or volume changes during testing can lead to unexpected results.
 *
 * This class monitors any changes related to peripherals and volume during the test execution to
 * ensure test validity.
 */
public class FrequencyTestObserver extends AudioDeviceCallback {
  private static final String TAG = "TestSupervisor";
  private volatile boolean isVolumeChanged = false;
  private volatile int expectedLevel;
  private volatile boolean isDeviceChanged;
  private final AudioManager audioManager;
  private int streamType;
  private Timer timer;
  private final int checkPeriodMs = 200;

  public FrequencyTestObserver(AudioManager audioManager) {
    this.audioManager = audioManager;
    this.isDeviceChanged = false;
    audioManager.registerAudioDeviceCallback(this, null);
  }

  public void start(int streamType) {
    this.streamType = streamType;
    expectedLevel = audioManager.getStreamVolume(streamType);
    isVolumeChanged = false;
    isDeviceChanged = false;
    timer = new Timer();
    timer.schedule(
        new TimerTask() {
          @Override
          public void run() {
            check();
          }
        },
        0,
        checkPeriodMs);
  }

  public void stop() {
    if (timer != null) {
      timer.cancel();
      timer = null;
    }
  }

  public void release() {
    stop();
    audioManager.unregisterAudioDeviceCallback(this);
  }

  private void check() {
    int currentLevel = audioManager.getStreamVolume(streamType);
    if (currentLevel != expectedLevel) {
      isVolumeChanged = true;
    }
  }

  public boolean isVolumeChanged() {
    return isVolumeChanged;
  }

  public boolean isDeviceChanged() {
    return this.isDeviceChanged;
  }

  @Override
  public void onAudioDevicesAdded(AudioDeviceInfo[] devices) {
    if (timer != null) {
      this.isDeviceChanged = true;
      for (AudioDeviceInfo device : devices) {
        Log.d(TAG, "Detected a new device of type " + device.getType());
      }
    }
  }

  @Override
  public void onAudioDevicesRemoved(AudioDeviceInfo[] devices) {
    if (timer != null) {
      this.isDeviceChanged = true;
      for (AudioDeviceInfo device : devices) {
        Log.d(TAG, "Disconnected to a device of type " + device.getType());
      }
    }
  }
}
