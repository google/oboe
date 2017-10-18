# Oboe
**Oboe is not yet ready for use. This repo is intended for Oboe developers only.**

Oboe is a C++ library which makes it easy to build high-performance audio apps on Android. It was created primarily to allow developers to target a single audio API, instead of both OpenSL ES (API 9+) and AAudio (API 26+).

To get started please read the [getting started guide](GettingStarted.md).

Oboe is not an official Google product.

**Note:** This version of Oboe only supports playback (output) streams. Support for recording (input) streams is in active development.

## Features
- Compatible with API 16 onwards - runs on 99% of Android devices
- Chooses the audio API (OpenSL ES on API 16+ or AAudio on API 26+) which will give the best audio performance on the target Android device
- Automatic latency tuning
- Modern C++ allowing you to write clean, elegant code

## Requirements
To build Oboe you will need the [Android NDK](https://developer.android.com/ndk/index.html) r16 or above

## Documentation
- [Getting Started Guide](GettingStarted.md)
- [Full Guide to Oboe](FullGuide.md)

## Sample code
Example apps can be found in the [Android high-performance audio repository](https://github.com/googlesamples/android-audio-high-performance/tree/master/oboe)

## Contributing
We would love to receive your pull requests. Before we can though we need you to read the [contributing](CONTRIBUTING.md) guidelines.
