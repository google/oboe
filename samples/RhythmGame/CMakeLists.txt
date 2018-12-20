cmake_minimum_required(VERSION 3.4.1)

include_directories(third_party)

include_directories(src/main/cpp/)

add_library( native-lib
             SHARED

             # main game files
             src/main/cpp/native-lib.cpp
             src/main/cpp/Game.cpp

             # audio engine
             src/main/cpp/audio/AAssetDataSource.cpp
             src/main/cpp/audio/Player.cpp

             # UI engine
             src/main/cpp/ui/OpenGLFunctions.cpp

             # utility functions
             src/main/cpp/utils/logging.h
             src/main/cpp/utils/UtilityFunctions.cpp

             )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( native-lib
                       log
                       android
                       oboe
                       GLESv2
                       )

# Set the path to the Oboe directory.
set (OBOE_DIR ../..)

# Add the Oboe library as a subdirectory in your project.
add_subdirectory (${OBOE_DIR} ./oboe-bin)

# Specify the path to the Oboe header files.
include_directories (${OBOE_DIR}/include ${OBOE_DIR}/samples)

# Enable optimization flags: if having problems with source level debugging,
# disable -Ofast ( and debug ), re-enable after done debugging.
target_compile_options(native-lib
    PRIVATE -std=c++14 -Wall -Werror "$<$<CONFIG:RELEASE>:-Ofast>")
