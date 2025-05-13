#
# Copyright 2025 The Android Open Source Project
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

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.31.6)

# Require C++17 compiler with no extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

link_directories(${CMAKE_CURRENT_LIST_DIR}/..)

find_library( # Sets the name of the path variable.
        log-lib
        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

### INCLUDE OBOE LIBRARY ###
# Set the path to the Oboe library directory
set(OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

# Add the Oboe library as a subproject. Since Oboe is an out-of-tree source library we must also
# specify a binary directory
add_subdirectory(${OBOE_DIR} ./oboe-bin)

# Specify the path to the Oboe header files and the source.
include_directories(
        ${OBOE_DIR}/include
        ${OBOE_DIR}/src
        ${OBOE_DIR}/src/flowgraph
)
### END OBOE INCLUDE SECTION ###

# App specific sources
set(APP_SOURCES
        PowerPlayJNI.cpp
)

# Build the powerplay (native) library
file(GLOB_RECURSE app_native_sources src/main/cpp/*)
add_library(
        powerplay SHARED ${app_native_sources}
)

# Enable optimization flags: if having problems with source level debugging,
# disable -Ofast ( and debug ), re-enable after done debugging.
target_compile_options(powerplay PRIVATE -Wall -Werror "$<$<CONFIG:RELEASE>:-O3 -ffast-math>")

target_link_libraries(powerplay oboe atomic ${log-lib})
target_link_options(powerplay PRIVATE "-Wl,-z,max-page-size=16384")
