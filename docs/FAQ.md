# Frequently Asked Questions (FAQ)

## Can I write audio data to Oboe from Java?

You could write JNI code to do this. But if you are generating your audio in Java then
you will get better performance using the Java AudioTrack class. The Java AudioTrack can be 
created with low latency using the AudioTrack.Builder method 
setPerformanceMode([AudioTrack.PERFORMANCE_MODE_LOW_LATENCY](https://developer.android.com/reference/android/media/AudioTrack#PERFORMANCE_MODE_LOW_LATENCY)).
You can dynamically tune the latency just like in Oboe.
Also you can use blocking writes with the Java AudioTrack and still get a low latency stream.
Oboe requires a callback to get a low latency stream and that does not work well with Java.

Note that AudioTrack.PERFORMANCE_MODE_LOW_LATENCY was added in API 26, For API 24 or 25 use AudioAttributes.FLAG_LOW_LATENCY. That was deprecated but will still work with later APIs.

## Can I use Oboe to play compressed audio files, such as MP3 or AAC?

Oboe only works with PCM data. It does not include any extraction or decoding classes. For this you can use:

1) [FFmpeg](https://www.ffmpeg.org/) - very fast decoding speeds, but can be difficult to configure and compile. [There's a good article on compiling FFmpeg 4.0 here](https://medium.com/@karthikcodes1999/cross-compiling-ffmpeg-4-0-for-android-b988326f16f2).
2) [The NDK media classes](https://developer.android.com/ndk/reference/group/media), specifically `NdkMediaExtractor` and `NdkMediaCodec` - they're approximately 10X slower than FFmpeg but ship with Android. [Code sample here](https://github.com/googlesamples/android-ndk/tree/master/native-codec). 

If you don't need the lowest possible audio latency you may want to investigate using the following Java/Kotlin APIs which support playback of compressed audio files: 

- [MediaPlayer](https://developer.android.com/reference/android/media/MediaPlayer)
- [SoundPool](https://developer.android.com/reference/android/media/SoundPool)

## My question isn't listed, where can I ask it?
Please ask questions on [Stack Overflow](https://stackoverflow.com/questions/ask) with the [Oboe tag](https://stackoverflow.com/tags/oboe). 
