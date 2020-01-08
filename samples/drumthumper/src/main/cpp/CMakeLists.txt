#
# Copyright 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.4.1)

# Pull in WavLib
set (WAVLIB_DIR ../../../../wavlib)
#message("WAVLIB_DIR = " + ${WAVLIB_DIR})

include(${WAVLIB_DIR}/src/main/cpp/CMakeLists.txt)

# Include the WavLib headers and shared sample code
include_directories(
        ${WAVLIB_DIR}/src/main/cpp
)

# App specific sources
set (APP_SOURCES
        DrumPlayerJNI.cpp
        )

# Build the drumthumper (native) library
add_library(drumthumper SHARED
        ${APP_SOURCES}
        )

# Specify the libraries needed for DrumThumper
target_link_libraries(drumthumper android log)

# Enable optimization flags: if having problems with source level debugging,
# disable -Ofast ( and debug ), re-enable after done debugging.
target_compile_options(drumthumper PRIVATE -Wall -Werror "$<$<CONFIG:RELEASE>:-Ofast>")

target_link_libraries( # Specifies the target library.
        drumthumper

        -Wl,--whole-archive
        wavlib
        -Wl,--no-whole-archive

        # Links the target library to the log library
        # included in the NDK.
        log)