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

# Script to build Oboe for multiple Android ABIs and prepare them for distribution via CDep
#
# Ensure that ANDROID_NDK environment variable is set to your Android NDK location
# e.g. /Library/Android/sdk/ndk-bundle

#!/bin/bash

if [ -z "$ANDROID_NDK" ]; then
  echo "Please set ANDROID_NDK to the Android NDK folder"
  exit 1
fi

# Directories, paths and filenames
BUILD_DIR=build
CDEP_UPLOAD_DIR=${BUILD_DIR}/upload
CDEP_UPLOAD_PATH=`pwd`/${CDEP_UPLOAD_DIR}
CDEP_MANIFEST_FILE=${CDEP_UPLOAD_PATH}/cdep-manifest.yml

ANDROID_SYSTEM_VERSION=26

mkdir -p ${CDEP_UPLOAD_DIR}

# Zip the headers
zip -r ${CDEP_UPLOAD_DIR}/oboe-headers.zip include/

# Make the CDep package descriptor
printf "%s\r\n" "coordinate:" > ${CDEP_MANIFEST_FILE}
printf "  %s\r\n" "groupId: com.github.google" >> ${CDEP_MANIFEST_FILE}
printf "  %s\r\n" "artifactId: oboe" >> ${CDEP_MANIFEST_FILE}
printf "  %s\r\n" "version: 0.9.0"  >> ${CDEP_MANIFEST_FILE}
printf "%s\r\n" "license:" >> ${CDEP_MANIFEST_FILE}
printf "  %s\r\n" "url: https://raw.githubusercontent.com/google/oboe/master/LICENSE" \
  >> ${CDEP_MANIFEST_FILE}

printf "%s\r\n" "interfaces:" >> ${CDEP_MANIFEST_FILE}
printf "  %s\r\n" "headers:" >> ${CDEP_MANIFEST_FILE}
printf "    %s\r\n" "file: oboe-headers.zip" >> ${CDEP_MANIFEST_FILE}
printf "    %s\r\n" "include: include" >> ${CDEP_MANIFEST_FILE}

printf "    sha256: " >> ${CDEP_MANIFEST_FILE}
shasum -a 256 ${CDEP_UPLOAD_DIR}/oboe-headers.zip | awk '{print $1}' >> ${CDEP_MANIFEST_FILE}

printf "    size: " >> ${CDEP_MANIFEST_FILE}
ls -l ${CDEP_UPLOAD_DIR}/oboe-headers.zip | awk '{print $5}' >> ${CDEP_MANIFEST_FILE}
printf "%s\r\n" "android:" >> ${CDEP_MANIFEST_FILE}
printf "  %s\r\n" "archives:" >> ${CDEP_MANIFEST_FILE}

CMAKE_ARGS="-H. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=clang \
  -DCMAKE_SYSTEM_NAME=Android \
  -DCMAKE_SYSTEM_VERSION=${ANDROID_SYSTEM_VERSION} \
  -DCMAKE_ANDROID_STL_TYPE=c++_static \
  -DCMAKE_ANDROID_NDK=$ANDROID_NDK \
  -DCMAKE_INSTALL_PREFIX=."

function build_oboe {

  ABI=$1
  ABI_BUILD_DIR=build/${ABI}
  STAGING_DIR=staging

  echo "Building Oboe for ${ABI}"

  mkdir -p ${ABI_BUILD_DIR} ${ABI_BUILD_DIR}/${STAGING_DIR}

  cmake -B${ABI_BUILD_DIR} \
        -DCMAKE_ANDROID_ARCH_ABI=${ABI} \
        -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${STAGING_DIR}/lib/${ABI} \
        ${CMAKE_ARGS}

  pushd ${ABI_BUILD_DIR}
    make -j5

    echo "Creating CDep package for ${ABI} ABI"
    pushd ${STAGING_DIR}

      zip -r ${CDEP_UPLOAD_PATH}/oboe-${ABI}.zip .

      # Output the library SHA and size into the CDep manifest
      printf "    %s\r\n" "- file: oboe-${ABI}.zip" >> ${CDEP_MANIFEST_FILE}

      printf "      sha256: " >> ${CDEP_MANIFEST_FILE}
      shasum -a 256 ${CDEP_UPLOAD_PATH}/oboe-${ABI}.zip | awk '{print $1}' >> ${CDEP_MANIFEST_FILE}

      printf "      size: " >> ${CDEP_MANIFEST_FILE}
      ls -l ${CDEP_UPLOAD_PATH}/oboe-${ABI}.zip | awk '{print $5}' >> ${CDEP_MANIFEST_FILE}

      printf "    %s\r\n" "  abi: ${ABI}" >> ${CDEP_MANIFEST_FILE}
      printf "    %s\r\n" "  platform: ${ANDROID_SYSTEM_VERSION}" >> ${CDEP_MANIFEST_FILE}
      printf "      libs: [liboboe.a]\r\n" >> ${CDEP_MANIFEST_FILE}

    popd
  popd
}

build_oboe armeabi
build_oboe armeabi-v7a
build_oboe arm64-v8a
build_oboe x86
build_oboe x86_64
build_oboe mips

# Currently unsupported ABIs
# build_oboe mips64

# Output a code example
printf "%s\r\n" "example: |" >> ${CDEP_MANIFEST_FILE}
printf "%s\r\n" "  #include <oboe/Oboe.h>" >> ${CDEP_MANIFEST_FILE}
printf "%s\r\n" "  void openStream() {" >> ${CDEP_MANIFEST_FILE}
printf "%s\r\n" "    StreamBuilder builder;" >> ${CDEP_MANIFEST_FILE}
printf "%s\r\n" "    Stream *stream;" >> ${CDEP_MANIFEST_FILE}
printf "%s\r\n" "    builder.openStream(&stream);" >> ${CDEP_MANIFEST_FILE}
printf "%s\r\n" "  }" >> ${CDEP_MANIFEST_FILE}


# Test package integrity
pushd ${BUILD_DIR}
git clone https://github.com/jomof/cdep-redist.git
cdep-redist/cdep wrapper
./cdep fetch ${CDEP_UPLOAD_PATH}/cdep-manifest.yml

# Create the example project
printf "%s\r\n" "builders: [cmake, cmakeExamples, ndk-build]" > cdep.yml
printf "%s\r\n" "dependencies:" >> cdep.yml
printf "%s\r\n" "- compile: ${CDEP_UPLOAD_PATH}/cdep-manifest.yml" >> cdep.yml
./cdep

# Build the example project

cmake \
  -H.cdep/examples/cmake/ \
  -Bbuild/examples \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=clang \
  -DCMAKE_SYSTEM_NAME=Android \
  -DCMAKE_SYSTEM_VERSION=${ANDROID_SYSTEM_VERSION} \
  -DCMAKE_ANDROID_STL_TYPE=c++_static \
  -DCMAKE_ANDROID_NDK=${ANDROID_NDK} \
  -DCMAKE_ANDROID_ARCH_ABI=armeabi \
  -DCMAKE_CXX_FLAGS="-Werror -Wall -std=c++11"

cmake --build build/examples

popd

