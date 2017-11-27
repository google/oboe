# Getting Started
The easiest way to start using Oboe is to build it from source by adding a few steps to an existing Android Studio project.

## Building Oboe
Start by cloning the Oboe repository: 

    git clone https://github.com/google/oboe

Open your app's `CMakeLists.txt`, this can be found under `External Build Files` in the Android project view. 

![CMakeLists.txt location in Android Studio](cmakelists-location-in-as.png "CMakeLists.txt location in Android Studio")

Now add the following build steps to `CMakeLists.txt`, making sure you update `/local/path/to/oboe` with your local Oboe repository directory:

    # Build the Oboe library
    set (OBOE_DIR /local/path/to/oboe)
    file (GLOB_RECURSE OBOE_SOURCES ${OBOE_DIR}/src/*)
    include_directories(${OBOE_DIR}/include ${OBOE_DIR}/src)
    add_library(oboe STATIC ${OBOE_SOURCES})

In the same file *after* your own library definition (by default it is named `native-lib`) add the dependencies for the Oboe and OpenSLES libraries:

    target_link_libraries(native-lib log oboe OpenSLES)

Here's a complete example `CMakeLists.txt` file:

    cmake_minimum_required(VERSION 3.4.1)

    # Build the Oboe library
    set (OBOE_DIR /Users/donturner/Code/workspace-android/oboe)
    file (GLOB_RECURSE OBOE_SOURCES ${OBOE_DIR}/src/*)
    include_directories(${OBOE_DIR}/include ${OBOE_DIR}/src)
    add_library(oboe STATIC ${OBOE_SOURCES})

    # Build our own native library
    add_library( native-lib SHARED src/main/cpp/native-lib.cpp )

    # Specify the libraries which our native library is dependent on
    target_link_libraries( native-lib log oboe OpenSLES )

Verify that your project builds correctly. If you have any issues building please [report them here](issues/new).

# Using Oboe
Once you've added Oboe to your project you can start using Oboe's features. The simplest, and probably most common thing you'll do in Oboe is to create an audio stream. 

## Creating an audio stream
Include the Oboe header:

    #include <oboe/Oboe.h>

Streams are built using an `StreamBuilder`. Create one like this:

    StreamBuilder builder;

Use the builder's set methods to set properties on the stream (you can read more about these properties in the [full guide](FullGuide.md)):

    builder.setDirection(OBOE_DIRECTION_OUTPUT);
    builder.setPerformanceMode(OBOE_PERFORMANCE_MODE_LOW_LATENCY);
    builder.setSharingMode(OBOE_SHARING_MODE_EXCLUSIVE);

Define an `StreamCallback` class to receive callbacks whenever the stream requires new data.

    class MyCallback : public StreamCallback {
    public:
        oboe_data_callback_result_t
        onAudioReady(Stream *audioStream, void *audioData, int32_t numFrames){
            generateSineWave(static_cast<float *>(audioData), numFrames);
            return OBOE_CALLBACK_RESULT_CONTINUE;
        }
    };

Supply this callback class to the builder:

    MyCallback myCallback;
    builder.setCallback(&myCallback);

Open the stream:

    Stream *stream;
    Result result = builder.openStream(&stream);

Check the result to make sure the stream was opened successfully. Oboe has many convenience methods for converting its types into human-readable strings, they all start with `Oboe_convert`:

    if (result != OK){
        LOGE("Failed to create stream. Error: %s", Oboe_convertResultToText(result));
    }

Note that this sample code uses the [logging macros from here](https://github.com/googlesamples/android-audio-high-performance/blob/master/debug-utils/logging_macros.h).

Check the properties of the created stream. The **format** is one property which you should check. The default is `float` on API 21+ and `int16_t` on API 20 or lower. This will dictate the `audioData` type in the `StreamCallback::onAudioReady` callback.

    oboe_audio_format_t format = stream->getFormat();
    LOGI("Stream format is %s", Oboe_convertAudioFormatToText(format));

Now start the stream. 

    stream->requestStart();

At this point you should start receiving callbacks.

When you are done with the stream you should close it:

    stream->close();

Note that `close()` is a blocking call which also stops the stream.

## Further information
- [Code samples](https://github.com/googlesamples/android-audio-high-performance/tree/master/oboe)
- [Full guide to Oboe](FullGuide.md)
