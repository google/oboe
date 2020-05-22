[Oboe Docs Home](README.md)

# Tech Note: Disconnected Streams

When Oboe is using **OpenSL ES**, and a headset is plugged in or out, then OpenSL ES will automatically switch between devices.
This is convenient but can cause problems because the new device may have different burst sizes and different latency.

When Oboe is using **AAudio**, and a headset is plugged in or out, then
the stream is no longer available and becomes "disconnected".
The app will then be notified in one of two ways. 

1) If the app is using a callback then the AudioStreamCallback object will be called.
It will launch a thread, which will call onErrorBeforeClose().
Then it stops and closes the stream.
Then onErrorAfterClose() will be called.
An app may choose to reopen a stream in the onErrorAfterClose() method.

2) If an app is using read()/write() calls then they will return an error when a disconnect occurs.
The app should then stop() and close() the stream.
An app may then choose to reopen a stream.

## Workaround for not Disconnecting Properly

On some versions of Android the disconnect message does not reach AAudio and the app will not
know that the device has changed. There is a "Test Disconnects" option in
[OboeTester](https://github.com/google/oboe/tree/master/apps/OboeTester/docs)
that can be used to diagnose this problem.

As a workaround you can listen for a Java [Intent.ACTION_HEADSET_PLUG](https://developer.android.com/reference/android/content/Intent#ACTION_HEADSET_PLUG),
which is fired when a head set is plugged in or out. If your min SDK is LOLLIPOP or later then you can use [AudioManager.ACTION_HEADSET_PLUG](https://developer.android.com/reference/android/media/AudioManager#ACTION_HEADSET_PLUG) instead.

    // Receive a broadcast Intent when a headset is plugged in or unplugged.
    public class PluginBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            // Close the stream if it was not disconnected.
        }
    }
    
    private BroadcastReceiver mPluginReceiver = new PluginBroadcastReceiver();
    
You can register for the Intent when your app resumes and unregister when it pauses.
    
    @Override
    public void onResume() {
        super.onResume();
        IntentFilter filter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);
        this.registerReceiver(mPluginReceiver, filter);
    }

    @Override
    public void onPause() {
        this.unregisterReceiver(mPluginReceiver);
        super.onPause();
    }
