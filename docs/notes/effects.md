[Tech Notes Home](README.md)

# Using Audio Effects with Oboe

## Overview

The Android Audio framework provides some effects processing that can be used by apps.
It is available through the Java or Kotlin
[AudioEffect API](https://developer.android.com/reference/android/media/audiofx/AudioEffect)

Another alternative is to do your own effects processing in your own app.

### Reasons to use the Android AudioEffect in the OS:
1. Functions are provided for you so they are easy to use.

### Reasons to do your own effects Processing:
1. They will work on all versions of Android. The AudioEffects can only be used with Oboe on Android 9 (Pie) and above. They are not supported for OpenSL ES.
2. You can customize the effects as needed.
3. You can get lower latency when you use your own effects. Using Android AudioEffects prevents you from getting a low latency path.

## Using Android AudioEffects

Oboe streams on Android 9 (Pie) and above can use the Java/Kotlin.
See [AudioEffect API](https://developer.android.com/reference/android/media/audiofx/AudioEffect)

The basic idea is to use Java or Kotlin to create a Session with Effects. 
Then associate your Oboe streams with the session by creating them with a SessionID.

In Java:

    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    int audioSessionId = audioManager.generateAudioSessionId();
    
Pass the audioSessionId to your C++ code using JNI. Then use it when opening your Oboe streams:

    builder->setSessionId(sessionId);

Note that these streams will probably not have low latency. So you may want to do your own effects processing.

## Using Third Party Affects Processing

There are many options for finding audio effects.

- [Music DSP Archive](http://www.musicdsp.org/en/latest/Effects/index.html)
- [Synthesis Toolkit in C++ (STK)](https://ccrma.stanford.edu/software/stk/index.html)
- [Cookbook for Biquad Filters, EQ, etc.](https://www.w3.org/2011/audio/audio-eq-cookbook.html)
- [Faust - language for generating effects, big library](https://faust.grame.fr/index.html)
- [DAFX Digital Audio Effects conference proceedings](http://dafx.de/)
