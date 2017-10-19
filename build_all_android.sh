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

# Script to build Oboe for multiple Android ABIs
#
# Ensure that ANDROID_NDK environment variable is set to your Android NDK location
# e.g. /Library/Android/sdk/ndk-bundle

#!/bin/bash

if [ -z "$ANDROID_NDK" ]; then
  echo "Please set ANDROID_NDK to the Android NDK folder"
  exit 1
fi

CMAKE_ARGS="-H. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=clang \
  -DCMAKE_SYSTEM_NAME=Android \
  -DCMAKE_SYSTEM_VERSION=21 \
  -DCMAKE_ANDROID_STL_TYPE=c++_static \
  -DCMAKE_ANDROID_NDK=$ANDROID_NDK \
  -DCMAKE_INSTALL_PREFIX=."

function build_oboe {

  ABI=$1
  BUILD_DIR=build/${ABI}

  echo "Building Oboe for ${ABI}"

  mkdir -p ${BUILD_DIR}

  cmake -B${BUILD_DIR} \
        -DCMAKE_ANDROID_ARCH_ABI=${ABI} \
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=../lib/${ABI} \
        ${CMAKE_ARGS}

  pushd ${BUILD_DIR}
  make -j5
  popd
}

build_oboe armeabi
build_oboe armeabi-v7a
build_oboe arm64-v8a
build_oboe x86
build_oboe x86_64

# MIPS ABIs are not supported at this time
# build_oboe mips
# build_oboe mips64
