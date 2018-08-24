cmake_minimum_required(VERSION 3.4.1)

# TODO: Remove this line and inherit it from the host environment
set(ANDROID_NDK $ENV{HOME}/Library/Android/sdk/ndk-bundle)

# Include GoogleTest library
set(GOOGLETEST_ROOT ${ANDROID_NDK}/sources/third_party/googletest)
add_library(gtest STATIC ${GOOGLETEST_ROOT}/src/gtest_main.cc ${GOOGLETEST_ROOT}/src/gtest-all.cc)
target_include_directories(gtest PRIVATE ${GOOGLETEST_ROOT})
target_include_directories(gtest PUBLIC ${GOOGLETEST_ROOT}/include)

# Include Oboe sources
set (OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${OBOE_DIR} ./oboe-bin)
include_directories(${OBOE_DIR}/include)

# include android headers
include_directories(${ANDROID_NDK}/sysroot/usr/include)

# Build the test binary
add_executable(testOboe
        testUtilities.cpp
        testStreamClosedMethods.cpp
        testStreamStates.cpp
		testStreamOpen.cpp
        testXRunBehaviour.cpp
        )
target_link_libraries(testOboe gtest oboe)
