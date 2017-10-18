# Getting Started

## Adding the Oboe library to your project

### Build using CMake
The Oboe library can easily be built by adding it to an Android Studio project.

Once you have cloned the Oboe repository, update your project's `app/CMakeLists.txt` as follows:

    # Build the Oboe library
    set (OBOE_DIR /local/path/to/oboe)
    file (GLOB_RECURSE OBOE_SOURCES ${OBOE_DIR}/src/*)
    include_directories(${OBOE_DIR}/include ${OBOE_DIR}/src)
    add_library(oboe STATIC ${OBOE_SOURCES})

### Add the Oboe library dependency
In the same file *after* your own library definition (named `native-lib` below) add the dependencies for the Oboe and OpenSLES libraries:

    target_link_libraries(native-lib log oboe OpenSLES)

### Update minimum SDK
Oboe requires a minimum API level of 16. Update your `app/build.gradle` file
accordingly:

    android {
        defaultConfig {
            ...
            minSdkVersion 16  
        }
    }

Now verify that your project builds correctly. If you have any issues building please [report them here](issues/new).

## Creating an audio stream
Include the Oboe header:

    #include <oboe/Oboe.h>

Streams are built using an `OboeStreamBuilder`. Create one like this:

    OboeStreamBuilder builder;

Use the builder's set methods to request properties on the stream (you can read more about these properties in the [full guide](FullGuide.md)):

    builder.setDirection(OBOE_DIRECTION_OUTPUT);
    builder.setPerformanceMode(OBOE_PERFORMANCE_MODE_LOW_LATENCY);
    builder.setSharingMode(OBOE_SHARING_MODE_EXCLUSIVE);

Define an `OboeStreamCallback` class to receive callbacks whenever the stream requires new data.

    class MyCallback : OboeStreamCallback {
    public:
        oboe_data_callback_result_t
        onAudioReady(OboeStream *audioStream, void *audioData, int32_t numFrames){
            generateSineWave(static_cast<float *>(audioData), numFrames);
            return OBOE_AUDIO_CALLBACK_RESULT_CONTINUE;
        }
    }

Supply this callback class to the builder:

    MyCallback myCallback;
    builder.setCallback(myCallback);

Open the stream:

    OboeStream *stream;
    oboe_result_t result = builder.openStream(&stream);

Check the result to make sure the stream was opened successfully. Oboe has many convenience methods for converting its types into human-readable strings, they all start with `Oboe_convert`:

    if (result != OBOE_OK){
        LOGE("Failed to create stream. Error: %s", Oboe_convertResultToText(result));
    }

Check the properties of the created stream. The **format** is one property which you should check. The default is `float` on API 21+ and `int16_t` on API 20 or lower. This will dictate the `audioData` type in the `OboeStreamCallback::onAudioReady` callback.

    oboe_audio_format_t format = stream->getFormat();
    LOGI("Stream format is %s", Oboe_convertAudioFormatToText(format));

Now start the stream. At this point you should start receiving callbacks:

    stream->requestStart();

When you are done with the stream you should stop and close it:

    stream->requestStop();
    stream->requestClose();

## Further information
- [Code samples](https://github.com/googlesamples/android-audio-high-performance/tree/master/oboe)
- [Full guide to Oboe](FullGuide.md)
