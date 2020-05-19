[Tech Notes Home](README.md)

# Assert in releaseBuffer()

There is a bug that can sometimes cause an assert in ClientProxy::releaseBuffer() when headsets are connected or disconnected. 
The bug was originally reported at: https://github.com/google/oboe/issues/535

## Platforms Affected

The crash happens primarily when using OpenSL ES. It does not happen when using AAudio MMAP because it
does not call releaseBuffer(). It can happen when using AAudio “Legacy”, through AudioFlinger.
But it is rare because AAudio generally disconnects the stream and stops callbacks before the device is rerouted.

It can occur on any Android device running Android version 10 (Q) or earlier.

## Root Cause

The sequence of events is:
1. audio callback starts by obtaining a buffer from the device
1. app is called to render audio
1. device routing change occurs during the callback
1. callback ends by releasing the buffer back to the new device

The method releaseBuffer() checks to make sure the device matches the one in ObtainBuffer() and asserts if they do not match.
This bug is tracked internally at: b/136268149

## Workarounds

The bug can generally be avoided by using AAudio on Android 8.1 or above.

We are currently investigating possible workarounds for OpenSL ES on earlier versions.

If the callback is very quick, then the window is small and the crash is less likely to not occur.
If the callback is taking a long time, because the computation is complex, then the window
is wide and the probability very high for hitting this bug.

## Reproduce the Bug

These steps will trigger the bug most of the time:

1. Install OboeTester 1.5.22 or later.
1. Enter in a Terminal window: adb logcat | grep releaseBuffer
1. Launch OboeTester
1. Click TEST OUTPUT
1. Select API: OpenSL ES
1. Click OPEN
1. Click START, you should hear a tone
1. Slide "Workload" fader slowly up until you hear bad glitches.
1. Plug in headphones.
 
You may see a message like this in the logcat:

    AudioTrackShared: releaseBuffer: mUnreleased out of range, !(stepCount:96 <= mUnreleased:0 <= mFrameCount:480), BufferSizeInFrames:480

# OEM Information

These patches are available in Q AOSP:
1. [AudioTrack](https://android-review.googlesource.com/c/platform/frameworks/av/+/1251871/)
1. [AudioRecord](https://android-review.googlesource.com/c/platform/frameworks/av/+/1251872/)
