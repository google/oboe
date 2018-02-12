cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -std=c++11")

# Specify directories which the compiler should look for headers
include_directories(
  include
  src
)

set (oboe_sources
        include/oboe/LatencyTuner.h
        include/oboe/Definitions.h
        include/oboe/Stream.h
        include/oboe/StreamBase.h
        include/oboe/StreamBuilder.h
        include/oboe/StreamCallback.h
        include/oboe/Utilities.h
        src/aaudio/AAudioLoader.cpp
        src/aaudio/StreamAAudio.cpp
        src/common/AudioClock.h
        src/common/OboeDebug.h
        src/common/LatencyTuner.cpp
        src/common/Stream.cpp
        src/common/StreamBuilder.cpp
        src/common/Utilities.cpp
        src/fifo/FifoBuffer.cpp
        src/fifo/FifoController.cpp
        src/fifo/FifoControllerBase.cpp
        src/fifo/FifoControllerIndirect.cpp
        src/opensles/StreamBuffered.cpp
        src/opensles/StreamOpenSLES.cpp
        src/opensles/OpenSLESUtilities.cpp
        )

add_library(oboe STATIC ${oboe_sources})