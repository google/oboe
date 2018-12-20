cmake_minimum_required(VERSION 3.4.1)


### INCLUDE OBOE LIBRARY ###

# Set the path to the Oboe library directory
set (OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Add the Oboe library as a subproject. Since Oboe is an out-of-tree source library we must also
# specify a binary directory
add_subdirectory(${OBOE_DIR} ./oboe-bin)

# Include the Oboe headers
include_directories(${OBOE_DIR}/include ${OBOE_DIR}/samples)

### END OBOE INCLUDE SECTION ###

add_library( megadrone SHARED
        src/main/cpp/native-lib.cpp
        src/main/cpp/AudioEngine.cpp
        ${OBOE_DIR}/samples/shared/Oscillator.cpp
        )

target_link_libraries( megadrone log oboe )

# Enable optimization flags: if having problems with source level debugging,
# disable -Ofast ( and debug ), re-enable it after done debugging.
target_compile_options(megadrone PRIVATE -Wall -Werror -Ofast)
