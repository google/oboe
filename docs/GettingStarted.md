# Adding Oboe to your project
There are two ways use Oboe in your Android Studio project: 

1) **Use the Oboe pre-built library binaries and headers** *(Experimental)*. Use this approach if you just want to use a stable version of the Oboe library in your project.

or

2) **Build Oboe from source.** Use this approach if you would like to debug or make changes to the Oboe source code and contribute back to the project.

## Option 1) Using pre-built binaries and headers
*This approach is currently experimental as it uses a preview version of Android Studio.*

Oboe is distributed as a [prefab](https://github.com/google/prefab) package via [Google Maven](https://maven.google.com/web/index.html) (search for "oboe"). [Prefab support was added](https://android-developers.googleblog.com/2020/02/native-dependencies-in-android-studio-40.html) to [Android Studio Preview 4.0 Canary 9](https://developer.android.com/studio/preview) so you'll need to be using this version of Android Studio or above. 

Add the oboe dependency to your app's `build.gradle` file. Replace "1.3.0" with the [latest stable version](https://github.com/google/oboe/releases/) of Oboe:

    dependencies {
        implementation 'com.google.oboe:oboe:1.3.0'
    }

Prefab isn't enabled by default so enable it by adding following to your `gradle.properties` file, which is in the root folder of your app:

    # Enables Prefab
    android.enablePrefab=true

    # Set the latest prefab release version (https://github.com/google/prefab/releases)
    android.prefabVersion=1.0.0

    # Only add if you are using AGP < 4.0.0-beta03. This is fixed in later versions.
    # Work around https://issuetracker.google.com/149575364
    android.enableParallelJsonGen=false
    

**Note:** Please check back regularly to see whether these workarounds are still required. This will ensure you don't get stuck on an unecessary Android Studio configuration or outdated Prefab version.

Include and link to oboe by updating your `CMakeLists.txt`: 

    find_package (oboe REQUIRED CONFIG)
    target_link_libraries(app oboe::oboe) # You may have other libraries here such as `log`.

Configure your app to use the shared STL by updating your `app/build.gradle`: 

    android { 
        defaultConfig { 
            externalNativeBuild {
                cmake {
                    arguments "-DANDROID_STL=c++_shared"
                }
	        }
	    }
    }

## Option 2) Building from source

### 1. Clone the github repository
Start by cloning the [latest stable release](https://github.com/google/oboe/releases/) of the Oboe repository, for example:

    git clone -b 1.3-stable https://github.com/google/oboe

**Make a note of the path which you cloned oboe into - you will need it shortly**

If you use git as your version control system, consider adding Oboe as a [submodule](https://gist.github.com/gitaarik/8735255)  (underneath your
cpp directory)

```
git submodule add https://github.com/google/oboe
```

This makes it easier to integrate updates to Oboe into your app, as well as contribute to the Oboe project.

### 2. Update CMakeLists.txt
Open your app's `CMakeLists.txt`. This can be found under `External Build Files` in the Android project view. If you don't have a `CMakeLists.txt` you will need to [add C++ support to your project](https://developer.android.com/studio/projects/add-native-code).

![CMakeLists.txt location in Android Studio](images/cmakelists-location-in-as.png "CMakeLists.txt location in Android Studio")

Now add the following commands to the end of `CMakeLists.txt`. **Remember to update `**PATH TO OBOE**` with your local Oboe path from the previous step**:

    # Set the path to the Oboe directory.
    set (OBOE_DIR ***PATH TO OBOE***)

    # Add the Oboe library as a subdirectory in your project.
    # add_subdirectory tells CMake to look in this directory to
    # compile oboe source files using oboe's CMake file.
    # ./oboe specifies where the compiled binaries will be stored
    add_subdirectory (${OBOE_DIR} ./oboe)

    # Specify the path to the Oboe header files.
    # This allows targets compiled with this CMake (application code)
    # to see public Oboe headers, in order to access its API.
    include_directories (${OBOE_DIR}/include)


In the same file find the [`target_link_libraries`](https://cmake.org/cmake/help/latest/command/target_link_libraries.html) command.
Add `oboe` to the list of libraries which your app's library depends on. For example:

    target_link_libraries(native-lib oboe)

Here's a complete example `CMakeLists.txt` file:

    cmake_minimum_required(VERSION 3.4.1)

    # Build our own native library
    add_library (native-lib SHARED native-lib.cpp )

    # Build the Oboe library
    set (OBOE_DIR ./oboe)
    add_subdirectory (${OBOE_DIR} ./oboe)

    # Make the Oboe public headers available to our app
    include_directories (${OBOE_DIR}/include)

    # Specify the libraries which our native library is dependent on, including Oboe
    target_link_libraries (native-lib log oboe)


Now go to `Build->Refresh Linked C++ Projects` to have Android Studio index the Oboe library.

Verify that your project builds correctly. If you have any issues building please [report them here](issues/new).

# Using Oboe
Once you've added Oboe to your project you can start using Oboe's features. The simplest, and probably most common thing you'll do in Oboe is to create an audio stream.

## Creating an audio stream
Include the Oboe header:

    #include <oboe/Oboe.h>
    
Streams are built using an `AudioStreamBuilder`. Create one like this:

    oboe::AudioStreamBuilder builder;

Use the builder's set methods to set properties on the stream (you can read more about these properties in the [full guide](FullGuide.md)):

    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(oboe::ChannelCount::Mono);

The builder's set methods return a pointer to the builder. So they can be easily chained:

```
oboe::AudioStreamBuilder builder;
builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
  ->setSharingMode(oboe::SharingMode::Exclusive)
  ->setCallback(myCallback)
  ->setFormat(oboe::AudioFormat::Float);
```

Define an `AudioStreamCallback` class to receive callbacks whenever the stream requires new data.

    class MyCallback : public oboe::AudioStreamCallback {
    public:
        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) {
            
            // We requested AudioFormat::Float so we assume we got it.
            // For production code always check what format
            // the stream has and cast to the appropriate type.
            auto *outputData = static_cast<float *>(audioData);
	    
            // Generate random numbers (white noise) centered around zero.
            const float amplitude = 0.2f;
            for (int i = 0; i < numFrames; ++i){
                outputData[i] = ((float)drand48() - 0.5f) * 2 * amplitude;
            }
	    
            return oboe::DataCallbackResult::Continue;
        }
    };

You can find examples of how to play sound using digital synthesis and pre-recorded audio in the [code samples](../samples).

Declare your callback somewhere that it won't get deleted while you are using it.

    MyCallback myCallback;

Supply this callback class to the builder:

    builder.setCallback(&myCallback);
    
Declare a ManagedStream. Make sure it is declared in an appropriate scope (e.g.the member of a managing class). Avoid declaring it as a global.
```
oboe::ManagedStream managedStream;
```
Open the stream:

    oboe::Result result = builder.openManagedStream(managedStream);

Check the result to make sure the stream was opened successfully. Oboe has a convenience method for converting its types into human-readable strings called `oboe::convertToText`:

    if (result != oboe::Result::OK) {
        LOGE("Failed to create stream. Error: %s", oboe::convertToText(result));
    }

Note that this sample code uses the [logging macros from here](https://github.com/googlesamples/android-audio-high-performance/blob/master/debug-utils/logging_macros.h).

## Playing audio
Check the properties of the created stream. If you did not specify a channelCount, sampleRate, or format then you need to 
query the stream to see what you got. The **format** property will dictate the `audioData` type in the `AudioStreamCallback::onAudioReady` callback. If you did specify any of those three properties then you will get what you requested.

    oboe::AudioFormat format = stream->getFormat();
    LOGI("AudioStream format is %s", oboe::convertToText(format));

Now start the stream.

    managedStream->requestStart();

At this point you should start receiving callbacks.

To stop receiving callbacks call
    
    managedStream->requestStop();

## Closing the stream
It is important to close your stream when you're not using it to avoid hogging audio resources which other apps could use. This is particularly true when using `SharingMode::Exclusive` because you might prevent other apps from obtaining a low latency audio stream.

Streams can be explicitly closed:

    stream->close();

`close()` is a blocking call which also stops the stream.

Streams can also be automatically closed when going out of scope:

	{
		ManagedStream mStream;
		AudioStreamBuilder().build(mStream);
		mStream->requestStart();
	} // Out of this scope the mStream has been automatically closed 
	
It is preferable to let the `ManagedStream` object go out of scope (or be explicitly deleted) when the app is no longer playing audio.
For apps which only play or record audio when they are in the foreground this is usually done when [`Activity.onPause()`](https://developer.android.com/guide/components/activities/activity-lifecycle#onpause) is called.

## Reconfiguring streams
In order to change the configuration of the stream, simply call `openManagedStream`
again. The existing stream is closed, destroyed and a new stream is built and
populates the `managedStream`.
```
// Modify the builder with some additional properties at runtime.
builder.setDeviceId(MY_DEVICE_ID);
// Re-open the stream with some additional config
// The old ManagedStream is automatically closed and deleted
builder.openManagedStream(managedStream);
```
The `ManagedStream` takes care of its own closure and destruction. If used in an
automatic allocation context (such as a member of a class), the stream does not
need to be closed or deleted manually. Make sure that the object which is responsible for
the `ManagedStream` (its enclosing class) goes out of scope whenever the app is no longer
playing or recording audio, such as when `Activity.onPause()` is called.


## Example

The following class is a complete implementation of a `ManagedStream`, which
renders a sine wave. Creating the class (e.g. through the JNI bridge) creates
and opens an Oboe stream which renders audio, and its destruction stops and
closes the stream.
```
#include <oboe/Oboe.h>
#include <math.h>

class OboeSinePlayer: public oboe::AudioStreamCallback {
public:


    OboeSinePlayer() {
        oboe::AudioStreamBuilder builder;
        // The builder set methods can be chained for convenience.
        builder.setSharingMode(oboe::SharingMode::Exclusive)
          ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
          ->setChannelCount(kChannelCount)
          ->setSampleRate(kSampleRate)
          ->setFormat(oboe::AudioFormat::Float)
          ->setCallback(this)
          ->openManagedStream(outStream);
        // Typically, start the stream after querying some stream information, as well as some input from the user
        outStream->requestStart();
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) override {
        float *floatData = (float *) audioData;
        for (int i = 0; i < numFrames; ++i) {
            float sampleValue = kAmplitude * sinf(mPhase);
            for (int j = 0; j < kChannelCount; j++) {
                floatData[i * kChannelCount + j] = sampleValue;
            }
            mPhase += mPhaseIncrement;
            if (mPhase >= kTwoPi) mPhase -= kTwoPi;
        }
        return oboe::DataCallbackResult::Continue;
    }

private:
    oboe::ManagedStream outStream;
    // Stream params
    static int constexpr kChannelCount = 2;
    static int constexpr kSampleRate = 48000;
    // Wave params, these could be instance variables in order to modify at runtime
    static float constexpr kAmplitude = 0.5f;
    static float constexpr kFrequency = 440;
    static float constexpr kPI = M_PI;
    static float constexpr kTwoPi = kPI * 2;
    static double constexpr mPhaseIncrement = kFrequency * kTwoPi / (double) kSampleRate;
    // Keeps track of where the wave is
    float mPhase = 0.0;
};
```
Note that this implementation computes  sine values at run-time for simplicity,
rather than pre-computing them.
Additionally, best practice is to implement a separate callback class, rather
than managing the stream and defining its callback in the same class.
This class also automatically starts the stream upon construction. Typically,
the stream is queried for information prior to being started (e.g. burst size),
and started upon user input.
For more examples on how to use `ManagedStream` look in the [samples](https://github.com/google/oboe/tree/master/samples) folder.

## Obtaining optimal latency
One of the goals of the Oboe library is to provide low latency audio streams on the widest range of hardware configurations.
When a stream is opened using AAudio, the optimal rate will be chosen unless the app requests a specific rate. The framesPerBurst is also provided by AAudio.

But OpenSL ES cannot determine those values. So applications should query them using Java and then pass them to Oboe. They will be used for OpenSL ES streams on older devices.

Here's a code sample showing how to set these default values. 

*MainActivity.java*

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1){
        AudioManager myAudioMgr = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        String sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
	    int defaultSampleRate = Integer.parseInt(sampleRateStr);
	    String framesPerBurstStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
	    int defaultFramesPerBurst = Integer.parseInt(framesPerBurstStr);

	    native_setDefaultStreamValues(defaultSampleRate, defaultFramesPerBurst);
	}

*jni-bridge.cpp*

	JNIEXPORT void JNICALL
	Java_com_google_sample_oboe_hellooboe_MainActivity_native_1setDefaultStreamValues(JNIEnv *env,
	                                                                                  jclass type,
	                                                                                  jint sampleRate,
	                                                                                  jint framesPerBurst) {
	    oboe::DefaultStreamValues::SampleRate = (int32_t) sampleRate;
	    oboe::DefaultStreamValues::FramesPerBurst = (int32_t) framesPerBurst;
	}

Note that the values from Java are for built-in audio devices. Peripheral devices, such as Bluetooth may need larger framesPerBurst.

# Further information
- [Code samples](https://github.com/google/oboe/tree/master/samples)
- [Full guide to Oboe](FullGuide.md)
