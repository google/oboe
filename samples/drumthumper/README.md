**DrumThumper**
==========
Oboe playback sample app.

## Abstract
**DrumThumper** (apolgies to [Chumbawamba](https://www.youtube.com/watch?v=2H5uWRjFsGc)) is a "Drum Pad" app which demonstrates best-practices for low-latency audio playback using the Android **Oboe** API.
**DrumThumper** consists of a set of trigger pad widgets and an optional UI for controlling the level and stereo placement of each of the virtual drums.
The audio samples are stored in application resources as WAV data. This is parsed and loaded (by routines in **parselib**) into memory blocks.
The audio samples are mixed and played by routines in **iolib**.

**DrumThumper** is written in a combination of Kotlin for the UI and JNI/C++ for the player components (to demonstrate accessing native code from a Kotlin or Java application).

## DrumThumper project structure
### Kotlin App Layer
Contains classes for the application logic and defines the methods for accessing the native data and player functionality.

### Native (C++) layer
Contains the implementation of the `native` (JNI) methods defined in the `DrumPlayer` (Kotlin) class.

### Dependent Libraries
* **iolib**
Classes for playing audio data.

* **parselib**
Classes for parsing and loading audio data from WAV resources.

## App
* DrumPlayer.kt
The Kotlin class which provides the audio playback functionality by interfacing with the native (C++) libraries.

* DrumThumperActivity.kt
The main application logic.

* TriggerPad.kt
An Android View subclass which implements the "trigger pad" UI widgets

## Native-interface (JNI)
* DrumPlayerJNI.cpp
 This is where all the access to the native functionality is implemented.
