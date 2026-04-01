cmake_minimum_required(VERSION 3.22.1)

project(OboeDJ)

# Pull in required libs
set(PARSELIB_DIR ../../../../parselib)
set(IOLIB_DIR ../../../../iolib)
set(OBOE_DIR ../../../../../)

add_subdirectory(${OBOE_DIR} ./oboe-bin)

# include folders
include_directories(
        ${OBOE_DIR}/include
        ${OBOE_DIR}/samples/shared
        ${PARSELIB_DIR}/src/main/cpp
        ${IOLIB_DIR}/src/main/cpp
        ${CMAKE_CURRENT_LIST_DIR}
)

# Include parselib & iolib CMake to build the static libraries
include(${PARSELIB_DIR}/src/main/cpp/CMakeLists.txt)
include(${IOLIB_DIR}/src/main/cpp/CMakeLists.txt)

# App specific sources
set(APP_SOURCES
        native-lib.cpp
        DJEngine.cpp
)

# Build the OboeDJ (native) library
add_library(
        oboedj SHARED ${APP_SOURCES}
)

# Enable optimization flags
target_compile_options(oboedj PRIVATE -Wall -Werror "$<$<CONFIG:RELEASE>:-O3 -ffast-math>")

target_link_libraries(
        oboedj
        -Wl,--whole-archive
        parselib
        iolib
        -Wl,--no-whole-archive
        oboe
        log
)

target_link_options(oboedj PRIVATE "-Wl,-z,max-page-size=16384")
