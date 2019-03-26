# Frequently Asked Questions (FAQ)

## Can I write audio data from Java to Oboe?

Oboe is a native library written in C++ which uses the Android NDK. To move data from Java to C++ you can use [JNI](https://developer.android.com/training/articles/perf-jni). 

That said, if you are generating your audio in Java you'll get better performance using the [Java AudioTrack class](https://developer.android.com/reference/android/media/AudioTrack). This can be 
created with low latency using the AudioTrack.Builder method [`setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)`](https://developer.android.com/reference/android/media/AudioTrack#PERFORMANCE_MODE_LOW_LATENCY).

You can dynamically tune the latency of the stream just like in Oboe using [`setBufferSizeInFrames(int)`](https://developer.android.com/reference/android/media/AudioTrack.html#setBufferSizeInFrames(int))
Also you can use blocking writes with the Java AudioTrack and still get a low latency stream.
Oboe requires a callback to get a low latency stream and that does not work well with Java.

Note that [`AudioTrack.PERFORMANCE_MODE_LOW_LATENCY`](https://developer.android.com/reference/android/media/AudioTrack#PERFORMANCE_MODE_LOW_LATENCY) was added in API 26, For API 24 or 25 use [`AudioAttributes.FLAG_LOW_LATENCY`](https://developer.android.com/reference/kotlin/android/media/AudioAttributes#flag_low_latency). That was deprecated but will still work with later APIs.

## Can I use Oboe to play compressed audio files, such as MP3 or AAC?
Oboe only works with PCM data. It does not include any extraction or decoding classes. However, the [RhythmGame sample](https://github.com/google/oboe/tree/master/samples/RhythmGame) includes extractors for both NDK and FFmpeg. 

For more information on using FFmpeg in your app [check out this article](https://medium.com/@donturner/using-ffmpeg-for-faster-audio-decoding-967894e94e71).

## Android Studio doesn't find the Oboe symbols, how can I fix this?
Start by ensuring that your project builds successfully. The main thing to do is ensure that the Oboe include paths are set correctly in your project's `CMakeLists.txt`. [Full instructions here](https://github.com/google/oboe/blob/master/docs/GettingStarted.md#2-update-cmakeliststxt).

If that doesn't fix it try the following: 

1) Invalidate the Android Studio cache by going to File->Invalidate Caches / Restart
2) Delete the contents of `$HOME/Library/Caches/AndroidStudio<version>`

We have had several reports of this happening and are keen to understand the root cause. If this happens to you please file an issue with your Android Studio version and we'll investigate further. 

## My question isn't listed, where can I ask it?
Please ask questions on [Stack Overflow](https://stackoverflow.com/questions/ask) with the [Oboe tag](https://stackoverflow.com/tags/oboe). 
