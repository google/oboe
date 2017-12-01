cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -std=c++14")

# Specify directories which the compiler should look for headers
include_directories(
  include
  src
)

set (oboe_sources
        include/oboe/LatencyTuner.h
        include/oboe/Definitions.h
        include/oboe/AudioStream.h
        include/oboe/AudioStreamBase.h
        include/oboe/AudioStreamBuilder.h
        include/oboe/AudioStreamCallback.h
        include/oboe/Utilities.h
        src/aaudio/AAudioLoader.cpp
        src/aaudio/AudioStreamAAudio.cpp
        src/common/AudioClock.h
        src/common/OboeDebug.h
        src/common/LatencyTuner.cpp
        src/common/AudioStream.cpp
        src/common/AudioStreamBuilder.cpp
        src/common/Utilities.cpp
        src/fifo/FifoBuffer.cpp
        src/fifo/FifoController.cpp
        src/fifo/FifoControllerBase.cpp
        src/fifo/FifoControllerIndirect.cpp
        src/opensles/AudioStreamBuffered.cpp
        src/opensles/AudioStreamOpenSLES.cpp
        src/opensles/OpenSLESUtilities.cpp
        )

add_library(oboe STATIC ${oboe_sources})