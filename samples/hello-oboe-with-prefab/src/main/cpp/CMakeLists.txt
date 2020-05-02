#
# Copyright 2017 The Android Open Source Project
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

# Set the path to the Oboe library directory
set (OBOE_DIR ../../../../../)

# Include the shared sample code
include_directories(${OBOE_DIR}/samples/shared)

# Debug utilities
set (DEBUG_UTILS_PATH "${OBOE_DIR}/samples/debug-utils")
set (DEBUG_UTILS_SOURCES ${DEBUG_UTILS_PATH}/trace.cpp)
include_directories(${DEBUG_UTILS_PATH})

# App specific sources
set (APP_SOURCES
    jni_bridge.cpp
    HelloOboeEngine.cpp
    SoundGenerator.cpp
    LatencyTuningCallback.cpp
)

# Build the libhello-oboe library
add_library(hello-oboe SHARED
            ${DEBUG_UTILS_SOURCES}
            ${APP_SOURCES}
            )

# Specify the libraries needed for hello-oboe
target_link_libraries(hello-oboe
                      android
                      log
                      )

# Enable optimization flags: if having problems with source level debugging,
# disable -Ofast ( and debug ), re-enable after done debugging.
target_compile_options(hello-oboe PRIVATE -Wall -Werror "$<$<CONFIG:RELEASE>:-Ofast>")
