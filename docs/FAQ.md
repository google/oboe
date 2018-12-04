# Frequently Asked Questions (FAQ)

## Can I write audio data from Java to Oboe?

Oboe is a native library written in C++ which uses the Android NDK. To move data from Java to C++ you can use [JNI](https://developer.android.com/training/articles/perf-jni). 

That said, if you are generating your audio in Java you'll get better performance using the [Java AudioTrack class](https://developer.android.com/reference/android/media/AudioTrack). This can be 
created with low latency using the AudioTrack.Builder method [`setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)`](https://developer.android.com/reference/android/media/AudioTrack#PERFORMANCE_MODE_LOW_LATENCY).

You can dynamically tune the latency of the stream just like in Oboe using [`setBufferSizeInFrames(int)`](https://developer.android.com/reference/android/media/AudioTrack.html#setBufferSizeInFrames(int))
Also you can use blocking writes with the Java AudioTrack and still get a low latency stream.
Oboe requires a callback to get a low latency stream and that does not work well with Java.

Note that [`AudioTrack.PERFORMANCE_MODE_LOW_LATENCY`](https://developer.android.com/reference/android/media/AudioTrack#PERFORMANCE_MODE_LOW_LATENCY) was added in API 26, For API 24 or 25 use [`AudioAttributes.FLAG_LOW_LATENCY`](https://developer.android.com/reference/kotlin/android/media/AudioAttributes#flag_low_latency). That was deprecated but will still work with later APIs.
