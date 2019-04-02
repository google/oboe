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
project(HelloCMakeOboe)

list(APPEND CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(BUILD_SHARED_LIBS "Build as shared library" ON)

# Thirdparty libraries are built using ExternalProject_Add(), which per default
# doesn't inherit any variables from the parent project. Thus, we set all
# necessary Android related stuff, which are as well listed in the official
# documentation: https://developer.android.com/ndk/guides/cmake.html
set(HelloCMakeOboe_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/install)
set(HelloCMakeOboe_DEFAULT_ARGS
  -DANDROID_ABI:STRING=${ANDROID_ABI}
  -DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL}
  -DANDROID_NDK:STRING=${ANDROID_NDK}
  -DANDROID_PLATFORM:STRING=${ANDROID_PLATFORM}
  -DANDROID_STL:STRING=${ANDROID_STL}
  -DANDROID_TOOLCHAIN:STRING=${ANDROID_TOOLCHAIN}
  -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
  -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
  -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
  -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE};-Ofast
  -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
  -DCMAKE_GENERATOR:STRING=${CMAKE_GENERATOR}
  -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
  -DCMAKE_PREFIX_PATH:PATH=${HelloCMakeOboe_INSTALL_PREFIX};${CMAKE_PREFIX_PATH}
  -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE})

# Setup project dependencies
include(ExternalProject)
include(External_Oboe)

set(SOURCES
  src/main/cpp/JNIBridge.cpp)

add_library(hello_cmake_oboe ${SOURCES})

target_link_libraries(hello_cmake_oboe
  PRIVATE Oboe::Oboe)

target_compile_options(hello_cmake_oboe
  PUBLIC $<$<CONFIG:RELEASE>:-Ofast>)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" )
  target_compile_options(hello_cmake_oboe PRIVATE -Wall -Wextra -pedantic)
endif()
