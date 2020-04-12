# Sample Rate Converter

This folder contains a sample rate converter, or "resampler".
It is part of [Oboe](https://github.com/google/oboe) but has no dependencies on Oboe.
So the contents of this folder can be used outside of Oboe.

The converter is based on a sinc function that has been windowed by a hyperbolic cosine.
We found this had fewer artifacts than the more traditional Kaiser window.

## Creating a Resampler

Include the main header for the resampler.

    #include "resampler/MultiChannelResampler.h"

Here is an example of creating a stereo resampler that will convert from 44100 to 48000 Hz.
Only do this once, when you open your stream. Then use the sample resampler to process multiple buffers.

    MultiChannelResampler *resampler = MultiChannelResampler::make(
            2, // channel count
            44100, // input sampleRate
            48000, // output sampleRate
            MultiChannelResampler::Medium); // conversion quality

Possible values for quality include { Fastest, Low, Medium, High, Best }.
Higher quality levels will sound better but consume more CPU because they have more taps in the filter.

## Fractional Frame Counts

Note that the number of output frames generated for a given number of input frames can vary.

For example, suppose you are converting from 44100 Hz to 48000 Hz and using an input buffer with 940 frames. If you calculate the number of output frames you get:

    940 * 48000 * 44100 = 1023.1292517...
    
You cannot generate a fractional number of frames. So the resampler will sometimes generate 1023 frames and sometimes 1024 frames. On average it will generate 1023.1292517 frames. The resampler stores the fraction internally and keeps track of when to consume or generate a frame.

You can either use a fixed number of input frames or a fixed number of output frames. The other frame count will vary.

## Calling the Resampler with a fixed number of OUTPUT frames

In this example, suppose we have a fixed number of output frames and a variable number of input frames.

Assume you start with these variables and a method that returns the next input frame:

    float *outputBuffer;     // multi-channel buffer to be filled
    int    numOutputFrames;  // number of frames of output
    
The resampler has a method isWriteNeeded() that tells you whether to write to or read from the resampler.

    int outputFramesLeft = numOutputFrames;
    while (outputFramesLeft > 0) {
        if(resampler->isWriteNeeded()) {
            const float *frame = getNextInputFrame(); // you provide this
            resampler->writeNextFrame(frame);
        } else {
            resampler->readNextFrame(outputBuffer);
            outputBuffer += channelCount;
            outputFramesLeft--;
        }
    }

## Calling the Resampler with a fixed number of INPUT frames

In this example, suppose we have a fixed number of input frames and a variable number of output frames.

Assume you start with these variables:

    float *inputBuffer;     // multi-channel buffer to be consumed
    float *outputBuffer;    // multi-channel buffer to be filled
    int    numInputFrames;  // number of frames of input
    int    numOutputFrames = 0;
    int    channelCount;    // 1 for mono, 2 for stereo

    int inputFramesLeft = numInputFrames;
    while (inputFramesLeft > 0) {
        if(resampler->isWriteNeeded()) {
            resampler->writeNextFrame(inputBuffer);
            inputBuffer += channelCount;
            inputFramesLeft--;
        } else {
            resampler->readNextFrame(outputBuffer);
            outputBuffer += channelCount;
            numOutputFrames++;
        }
    }

## Deleting the Resampler

When you are done, you should delete the Resampler to avoid a memory leak.

    delete resampler;
    
