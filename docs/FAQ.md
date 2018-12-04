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
