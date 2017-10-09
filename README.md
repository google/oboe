# Oboe
Oboe is a C++ library which makes it easy to build high-performance audio apps
on Android. It's a C++ wrapper for the AAudio and OpenSL ES audio APIs. It runs
on Android versions from Jelly Bean 4.1 (API 16) upwards.

On Oreo (API 26) or later versions, Oboe can use AAudio or OpenSL ES.
On Nougat (API 25) or earlier versions, Oboe will only use OpenSL ES.

This is not an official Google product.

## Prerequisites
- Android Studio 2.3.3 or above

## Adding Oboe to your Android Studio project
1. Add the Oboe source code into `$PROJECT_DIR/app/src/main/cpp/oboe`. You can
either use `git clone` or if your project is already a git repository you can
add Oboe as a submodule: `git submodule add github.com/google/oboe`.

2. Update your project's `app/CMakeLists.txt` file to include the Oboe source
and header files, as well as the OpenSLES library by adding the following lines:

    `include_directories(src/main/cpp/oboe/include src/main/cpp/oboe/src)
    file(GLOB_RECURSE app_native_sources src/main/cpp/* src/main/cpp/oboe/src/*)
    add_library(native-lib SHARED ${app_native_sources})
    target_link_libraries(native-lib log OpenSLES)`

3. Check that your project builds.

Now the Oboe source is in your project it's really easy to
[contribute](CONTRIBUTING) modifications back to Oboe.

TODO: Add getting started guide, sample code and links to sample apps
