cmake_minimum_required(VERSION 3.4.1)

# Usually this file is called from run_tests.sh which requires the $ANDROID_NDK variable to be set so there's no need
# to set it here. Comments below are left in intentionally in case they're useful in future.
# This may work on Mac OS.
# set(ANDROID_NDK $ENV{HOME}/Library/Android/sdk/ndk-bundle)
# This may work on Linux.
# set(ANDROID_NDK $ENV{HOME}/Android/sdk/ndk-bundle)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -std=c++14 -fno-rtti")

# Include GoogleTest library
set(GOOGLETEST_ROOT ${ANDROID_NDK}/sources/third_party/googletest)
add_library(gtest STATIC ${GOOGLETEST_ROOT}/src/gtest_main.cc ${GOOGLETEST_ROOT}/src/gtest-all.cc)
target_include_directories(gtest PRIVATE ${GOOGLETEST_ROOT})
target_include_directories(gtest PUBLIC ${GOOGLETEST_ROOT}/include)

# Add the Oboe shared lib and headers 
add_library(oboe SHARED IMPORTED)
set_property(TARGET oboe PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../build/${ANDROID_ABI}/liboboe.so")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../include" "${CMAKE_CURRENT_SOURCE_DIR}/../src")

# Build the test binary
add_executable(
        testOboe
        testAAudio.cpp
        testUtilities.cpp
        testFlowgraph.cpp
        testStreamClosedMethods.cpp
        testStreamWaitState.cpp
        testXRunBehaviour.cpp
        testStreamOpen.cpp
        testStreamStates.cpp
        )

target_link_libraries(testOboe gtest oboe)
