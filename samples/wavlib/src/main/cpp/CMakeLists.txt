# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.4.1)

#PROJECT(wavlib C CXX)

message("CMAKE_CURRENT_LIST_DIR = " ${CMAKE_CURRENT_LIST_DIR})

message("HOME is " ${HOME})

# SET(NDK "")
message("NDK is " ${NDK})

# compiler flags
# -mhard-float -D_NDK_MATH_NO_SOFTFP=1
#SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -mhard-float -D_NDK_MATH_NO_SOFTFP=1" )

# include folders
include_directories(
    ${CMAKE_CURRENT_LIST_DIR}
    ../../../../shared
)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        wavlib

        # Sets the library as a static library.
        STATIC

        # Provides a relative path to your source file(s).
        # io/stream
        ${CMAKE_CURRENT_LIST_DIR}/io/stream/FileInputStream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/io/stream/InputStream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/io/stream/MemInputStream.cpp
        # io/wav
        ${CMAKE_CURRENT_LIST_DIR}/io/wav/AudioFormat.cpp
        ${CMAKE_CURRENT_LIST_DIR}/io/wav/WavChunkHeader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/io/wav/WavFmtChunkHeader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/io/wav/WavRIFFChunkHeader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/io/wav/WavStreamReader.cpp
        #include player
        ${CMAKE_CURRENT_LIST_DIR}/player/Player.cpp
)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib
              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
            wavlib

            # Links the target library to the log library
            # included in the NDK.
            ${log-lib} )
