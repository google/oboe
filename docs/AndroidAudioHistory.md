Android audio history
===

A list of important audio features, bugs, fixes and workarounds for various Android versions. 

### 10.0 Q - API 29
- Fixed: Setting capacity of Legacy input streams < 4096 can prevent use of FAST path. https://github.com/google/oboe/issues/183. also ag/7116429
- Add InputPreset:VoicePerformance for low latency recording.

### 9.0 Pie - API 28 (August 6, 2018)
- AAudio adds support for setUsage(), setSessionId(), setContentType(), setInputPreset() for builders.
- Regression bug: [AAudio] Headphone disconnect event not fired for MMAP streams. https://github.com/google/oboe/issues/252
- AAudio input streams with LOW_LATENCY will open a FAST path using INT16 and convert the data to FLOAT if needed. See: https://github.com/google/oboe/issues/276

### 8.1 Oreo MR1 - API 27
- Oboe uses AAudio by default.
- AAudio MMAP data path enabled on Pixel devices. PerformanceMode::Exclusive supported.
- Fixed: [AAudio] RefBase issue
- Fixed: Requesting a stereo recording stream can result in sub-optimal latency. 

### 8.0 Oreo - API 26 (August 21, 2017)
- [AAudio API introduced](https://developer.android.com/ndk/guides/audio/aaudio/aaudio)
- Bug: RefBase issue causes crash after stream closed. This why AAudio is not recommended for 8.0. Oboe will use OpenSL ES for 8.0 and earlier.
  https://github.com/google/oboe/issues/40
- Bug: Requesting a stereo recording stream can result in sub-optimal latency. [Details](https://issuetracker.google.com/issues/68666622)

### 7.1 Nougat MR1 - API 25
- OpenSL adds supports for setting and querying of PerformanceMode.

### 7.0 Nougat - API 24 (August 22, 2016)
- OpenSL method `acquireJavaProxy` added, which allows the Java AudioTrack object associated with playback to be obtained (which allows underrun count).

### 6.0 Marshmallow - API 23 (October 5, 2015)
- Floating point recording supported. But it does not allow a FAST "low latency" path.
- [MIDI API introduced](https://developer.android.com/reference/android/media/midi/package-summary)
- Sound output is broken on the API 23 emulator

### 5.0 Lollipop - API 21 (November 12, 2014)
- Floating point playback supported.





