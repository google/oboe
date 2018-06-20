cmake_minimum_required(VERSION 3.4.1)


### INCLUDE OBOE LIBRARY ###

# Set the path to the Oboe library directory
set (OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Add the Oboe library as a subproject. Since Oboe is an out-of-tree source library we must also
# specify a binary directory
add_subdirectory(${OBOE_DIR} ./oboe-bin)

# Include the Oboe headers
include_directories(${OBOE_DIR}/include)

### END OBOE INCLUDE SECTION ###


add_library( megadrone SHARED
        src/main/cpp/native-lib.cpp
        src/main/cpp/AudioEngine.cpp
        src/main/cpp/Oscillator.cpp
        )

target_link_libraries( megadrone log oboe )

# Make the Oboe public headers available to our app
include_directories (${OBOE_DIR}/include)

# Append compiler optimization flags to both the debug and release build variants
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Ofast")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")

