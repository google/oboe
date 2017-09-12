#!/bin/bash
# Script to build Oboe for multiple Android ABIs
#
# Ensure that ANDROID_NDK environment variable is set to your Android NDK location
# e.g. /Library/Android/sdk/ndk-bundle
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

  mkdir ${BUILD_DIR}

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
