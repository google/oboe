# Sample Rate Converter

This folder contains a sample rate converter, or "resampler".
It is part of [Oboe](https://github.com/google/oboe) but has no dependencies on Oboe.
So it can be used outside of Oboe.

The converter is based on a sinc function that has been windowed by a hyperbolic cosine.
We found this had fewer artifacts than the more traditional Kaiser window.

## Creating a Resampler

Include the main header for the resampler.

    #include "resampler/MultiChannelResampler.h"

Here is an example of creating a stereo resampler that will convert from 44100 to 48000 Hz.

    MultiChannelResampler *mResampler = MultiChannelResampler::make(
            2, // channel count
            44100, // input sampleRate
            48000, // output sampleRate
            MultiChannelResampler::Medium); // conversion quality

Possible values for quality include { Fastest, Low, Medium, High, Best }.
Higher quality levels will sound better but consume more CPU because they have more taps in the filter.

## Calling the Resampler

Assume you start with these variables and a method that returns the next input frame:

    float *outputBuffer;  // multi-channel buffer to be filled
    int numFrames;        // number of frames of output
    int32_t channelCount; // 1 for mono, 2 for stereo, etc.
    
The resampler has a method isWriteNeeded() that tells you whether to write to or read from the resampler.

    while (numFrames > 0) {
        if(mResampler->isWriteNeeded()) {
            const float *frame = getNextInputFrame(); // you provide this
            mResampler->writeNextFrame(frame);
        } else {
            mResampler->readNextFrame(outputBuffer);
            outputBuffer += channelCount;
            numFrames--;
        }
    }
    
