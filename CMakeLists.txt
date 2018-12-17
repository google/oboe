cmake_minimum_required(VERSION 3.4.1)

set (oboe_sources
        src/aaudio/AAudioLoader.cpp
        src/aaudio/AudioStreamAAudio.cpp
        src/common/LatencyTuner.cpp
        src/common/AudioStream.cpp
        src/common/AudioStreamBuilder.cpp
        src/common/Utilities.cpp
        src/fifo/FifoBuffer.cpp
        src/fifo/FifoController.cpp
        src/fifo/FifoControllerBase.cpp
        src/fifo/FifoControllerIndirect.cpp
        src/opensles/AudioInputStreamOpenSLES.cpp
        src/opensles/AudioOutputStreamOpenSLES.cpp
        src/opensles/AudioStreamBuffered.cpp
        src/opensles/AudioStreamOpenSLES.cpp
        src/opensles/EngineOpenSLES.cpp
        src/opensles/OpenSLESUtilities.cpp
        src/opensles/OutputMixerOpenSLES.cpp
        src/common/StabilizedCallback.cpp
		src/common/Trace.cpp
        )

add_library(oboe STATIC ${oboe_sources})

# Specify directories which the compiler should look for headers
target_include_directories(oboe
                           PRIVATE src
                           PUBLIC include)

# Compile Flags:
#     Enable -Werror when building debug config
#     Enable -Ofast
target_compile_options(oboe
    PRIVATE
    -std=c++11
    -Wall
    -Wextra-semi
    -Wshadow
    -Wshadow-field
    -Ofast
    "$<$<CONFIG:DEBUG>:-Werror>")

# Enable logging for debug builds
target_compile_definitions(oboe PUBLIC "$<$<CONFIG:DEBUG>:OBOE_ENABLE_LOGGING=1>")

target_link_libraries(oboe PRIVATE log OpenSLES)
