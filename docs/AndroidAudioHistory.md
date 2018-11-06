Android audio history
===
** work in progress **

A list of important audio features, bugs, fixes and workarounds for various Android versions. 

9.0 Pie - API 28 (August 6, 2018)
---
- Fixed: [AAudio] RefBase issue
- Regression bug: [AAudio] Headphone disconnect event not fired

8.1 Oreo MR1 - API 27 
---
- Oboe uses AAudio by default
- Fixed: [AAudio] RefBase issue
- Bug: [AAudio] Headphone disconnect event not fired

8.0 Oreo - API 26 (August 21, 2017)
---
- [AAudio API introduced](https://developer.android.com/ndk/guides/audio/aaudio/aaudio)
- Bug: RefBase issue causes crash after stream closed (philburk@ to add details)

6.1 Marshmallow MR1 - API 24
---
- OpenSL method `acquireJavaProxy` added which allows the Java AudioTrack object associated with playback to be obtained (which allows underrun count).

6.0 Marshmallow - API 23 (October 5, 2015)
---
- Floating point recording support
- [MIDI API introduced](https://developer.android.com/reference/android/media/midi/package-summary)

5.0 Lollipop - API 21 (November 12, 2014)
---
- Floating point playback support





