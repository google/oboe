#!/bin/bash

set -e
#  echo y | sdkmanager --list

# List of the SDK component to install
declare sdk_components=(
   "platform-tools"
   "tools"
   "extras;google;m2repository"
   "extras;android;m2repository"

   "ndk-bundle"
   "cmake;3.6.4111459"
   "lldb;3.1")

touch ~/.android/repositories.cfg
for id in "${sdk_components[@]}"; do 
    echo y | sdkmanager --sdk_root=$ANDROID_HOME $id >/dev/null
done

echo y | sdkmanager --update > /dev/null

set +e