[Tech Notes Home](README.md)

# Assert in releaseBuffer()

There is a bug that can sometimes cause an assert in ClientProxy or AudioTrackShared::releaseBuffer() when headsets are connected or disconnected. 
The bug was originally reported at: https://github.com/google/oboe/issues/535

You will see signatures like this in the logcat:

    F AudioTrackShared: releaseBuffer: mUnreleased out of range, !(stepCount:96 <= mUnreleased:0 <= mFrameCount:480), BufferSizeInFrames:480

## Platforms Affected

Android version 10 (Q) or earlier.

Oboe version 1.4.0 or earlier when using OpenSLES with an OUTPUT stream callback.

OR any version of Oboe if:
* Oboe is using using OpenSL ES or a non-MMAP Legacy AAudio stream
* AND you call stream->getFramesRead() or stream->getTimestamp(...) from inside
an OUTPUT stream callback,

It does **not** happen when Oboe uses AAudio MMAP because it does not call releaseBuffer().

## Workarounds

1. Use Oboe 1.4.1 or later.
1. Do not call stream->getFramesRead() or stream->getTimestamp() from inside the callback of an OUTPUT stream. If you absolutely must, then call them at the beginning of your callback to reduce the probability of a crash.

Here is a [fix in Oboe 1.4.1](https://github.com/google/oboe/pull/863) that removed a call to getPosition().

## Root Cause

The sequence of events is:
1. AudioFlinger AudioTrack obtains a buffer from the audio device
1. user plugs in headphones, which invalidates the audio device
1. app is called (callback) to render audio using the buffer
1. the app or Oboe calls getFramesRead() or getTimestamp(), which calls down to AudioTrack::getPosition() or AudioTrack::getTimestamp()
1. device routing change occurs because the audio device is [invalid](https://cs.android.com/android/platform/superproject/+/master:frameworks/av/media/libaudioclient/AudioTrack.cpp;l=1239;drc=48e98cf8dbd9fa212a0e129822929dc40e6c3898)
1. callback ends by releasing the buffer back to a different device
1. AudioTrackShared::releaseBuffer() checks to make sure the device matches the one in ObtainBuffer() and asserts if they do not match.

Oboe, before V1.4.1, would update the server position in its callback. This called getPosition() in OpenSL ES, which called AudioTrack::getPosition().

The probability of the assert() is proportional to the time that the CPU spends between obtaining a buffer and calling restoreTrack_l().

This bug is tracked internally at: b/136268149

## Reproduce the Bug

These steps will trigger the bug most of the time:

1. Install OboeTester 1.5.22 or later, with Oboe < 1.4.1
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
