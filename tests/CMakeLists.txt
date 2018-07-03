cmake_minimum_required(VERSION 3.4.1)

# Include GoogleTest library
set(GOOGLETEST_ROOT ${ANDROID_NDK}/sources/third_party/googletest)
add_library(gtest STATIC ${GOOGLETEST_ROOT}/src/gtest_main.cc ${GOOGLETEST_ROOT}/src/gtest-all.cc)
target_include_directories(gtest PRIVATE ${GOOGLETEST_ROOT})
target_include_directories(gtest PUBLIC ${GOOGLETEST_ROOT}/include)

# Include Oboe sources
set (OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${OBOE_DIR} ./oboe-bin)
include_directories(${OBOE_DIR}/include)

# Build the test binary
add_executable(testOboe
        testUtilities.cpp
        testStreamClosedMethods.cpp
        )
target_link_libraries(testOboe gtest oboe)