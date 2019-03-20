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

if(${USE_FFMPEG})

    MESSAGE(STATUS "Using FFmpeg extractor")

    add_definitions(-DUSE_FFMPEG=1)
    target_sources( native-lib PRIVATE src/main/cpp/audio/FFMpegExtractor.cpp )

    # Add the local path to FFmpeg, you can use the ${ANDROID_ABI} variable to specify the ABI name
    # e.g. /Users/donturner/Code/ffmpeg/build/${ANDROID_ABI}
    set(FFMPEG_DIR "/path/to/ffmpeg")

    include_directories(native-lib ${FFMPEG_DIR}/include)

    add_library( avformat SHARED IMPORTED)
    set_target_properties(avformat PROPERTIES IMPORTED_LOCATION
            ${FFMPEG_DIR}/lib/libavformat.so)
    add_library( avutil SHARED IMPORTED)
    set_target_properties(avutil PROPERTIES IMPORTED_LOCATION
            ${FFMPEG_DIR}/lib/libavutil.so)
    add_library( avcodec SHARED IMPORTED)
    set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION
            ${FFMPEG_DIR}/lib/libavcodec.so)
    add_library( swresample SHARED IMPORTED)
    set_target_properties(swresample PROPERTIES IMPORTED_LOCATION
            ${FFMPEG_DIR}/lib/libswresample.so)

    target_link_libraries( native-lib avformat avutil avcodec swresample)

else()
    MESSAGE(STATUS "Using NDK media extractor")
    add_definitions(-DUSE_FFMPEG=0)
    target_sources( native-lib PRIVATE src/main/cpp/audio/NDKExtractor.cpp )
    target_link_libraries(native-lib mediandk)
endif()

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
