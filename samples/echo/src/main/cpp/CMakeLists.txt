#
# Copyright 2018 The Android Open Source Project
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
project(echo LANGUAGES C CXX)

get_filename_component(SAMPLE_ROOT_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../.. ABSOLUTE)

### INCLUDE OBOE LIBRARY ###
set (OBOE_DIR ${SAMPLE_ROOT_DIR}/..)
add_subdirectory(${OBOE_DIR} ./oboe-bin)

add_library(echo
    SHARED
        EchoAudioEngine.cpp
        jni_bridge.cpp
        AudioEffect.cpp
        ${SAMPLE_ROOT_DIR}/debug-utils/trace.cpp
        ${SAMPLE_ROOT_DIR}/common/audio_common.cpp)
target_include_directories(echo
    PRIVATE
        ${SAMPLE_ROOT_DIR}/common
        ${SAMPLE_ROOT_DIR}/debug-utils
        ${OBOE_DIR}/include)
target_link_libraries(echo
    PRIVATE
        oboe
        android
        atomic
        log)

target_compile_options(echo
    PRIVATE
        -Wall -Werror)
