# Oboe
**Oboe is currently in developer preview.**

Oboe is a C++ library which makes it easy to build high-performance audio apps on Android. It was created primarily to allow developers to target a simplified API that works across multiple API levels back to API level 16 (Jelly Bean).

[Get started with Oboe here](GettingStarted.md).

# Projects using Oboe
- [JUCE middleware framework](https://juce.com/)
- CSound for Android. [App](https://play.google.com/store/apps/details?id=com.csounds.Csound6), [Source](https://github.com/gogins/csound-extended/blob/develop/CsoundForAndroid/CsoundAndroid/jni/csound_oboe.hpp)

Want your project added? [File an issue](https://github.com/google/oboe/issues/new) with your project name and URL. 

## Features
- Compatible with API 16 onwards - runs on 99% of Android devices
- Chooses the audio API (OpenSL ES on API 16+ or AAudio on API 27+) which will give the best audio performance on the target Android device
- Automatic latency tuning
- Modern C++ allowing you to write clean, elegant code

## Requirements
To build Oboe you will need the [Android NDK](https://developer.android.com/ndk/index.html) r15 or above

## Documentation
- [Getting Started Guide](GettingStarted.md)
- [Full Guide to Oboe](FullGuide.md)

## Sample code
Sample apps can be found in the [samples directory](samples). Also check out the [Rhythm Game codelab](https://codelabs.developers.google.com/codelabs/musicalgame-using-oboe/index.html#0).

### Third party sample code
- [Ableton Link integration demo](https://github.com/jbloit/AndroidLinkAudio) (author: jbloit)

## Contributing
We would love to receive your pull requests. Before we can though, please read the [contributing](CONTRIBUTING.md) guidelines.

## Version history

- 18th January 2018 - v0.10 Add support for input (recording) streams
- 18th October 2017 - v0.9 Initial developer preview
