#!/bin/bash
# Handler for before_install step in CI
# Functionalities:
# 1.  install sdkmanager manually
# 2.  TODO: install whole new SDK
# 3.  TODO: point sdk location to our installed one ( not the travis's )
# Side Effect:
#     $CI_SDK_DIR is left behind on the disk: could not delete.

set -e

CI_SDK_DIR=$HOME/ci_sdk
SDK_TOOL_FILENAME=sdk-tools-linux-4333796.zip

mkdir -p $CI_SDK_DIR
wget -q "https://dl.google.com/android/repository/$SDK_TOOL_FILENAME"
unzip  -o -qq -d $CI_SDK_DIR $SDK_TOOL_FILENAME > /dev/null
export PATH=$CI_SDK_DIR/tools:$CI_SDK_DIR/tools/bin:$PATH

rm -f $SDK_TOOL_FILENAME

set +e