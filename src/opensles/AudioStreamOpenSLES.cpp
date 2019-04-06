/* Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/types.h>
#include <cassert>
#include <android/log.h>


#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <oboe/AudioStream.h>
#include <common/AudioClock.h>

#include "common/OboeDebug.h"
#include "oboe/AudioStreamBuilder.h"
#include "AudioStreamOpenSLES.h"
#include "OpenSLESUtilities.h"

#ifndef NULL
#define NULL 0
#endif

using namespace oboe;

AudioStreamOpenSLES::AudioStreamOpenSLES(const AudioStreamBuilder &builder)
    : AudioStreamBuffered(builder) {
    // OpenSL ES does not support device IDs. So overwrite value from builder.
    mDeviceId = kUnspecified;
    // OpenSL ES does not support session IDs. So overwrite value from builder.
    mSessionId = SessionId::None;
}

AudioStreamOpenSLES::~AudioStreamOpenSLES() {
    delete[] mCallbackBuffer;
}

constexpr SLuint32  kAudioChannelCountMax = 30;
constexpr SLuint32  SL_ANDROID_UNKNOWN_CHANNELMASK  = 0; // Matches name used internally.

SLuint32 AudioStreamOpenSLES::channelCountToChannelMaskDefault(int channelCount) {
    if (channelCount > kAudioChannelCountMax) {
        return SL_ANDROID_UNKNOWN_CHANNELMASK;
    } else {
        SLuint32 bitfield = (1 << channelCount) - 1;

        // Check for OS at run-time.
        if(getSdkVersion() >= __ANDROID_API_N__) {
            return SL_ANDROID_MAKE_INDEXED_CHANNEL_MASK(bitfield);
        } else {
            // Indexed channels masks were added in N.
            // For before N, the best we can do is use a positional channel mask.
            return bitfield;
        }
    }
}

static bool s_isLittleEndian() {
    static uint32_t value = 1;
    return (*reinterpret_cast<uint8_t *>(&value) == 1);  // Does address point to LSB?
}

SLuint32 AudioStreamOpenSLES::getDefaultByteOrder() {
    return s_isLittleEndian() ? SL_BYTEORDER_LITTLEENDIAN : SL_BYTEORDER_BIGENDIAN;
}

Result AudioStreamOpenSLES::open() {

    LOGI("AudioStreamOpenSLES::open(chans:%d, rate:%d)",
                        mChannelCount, mSampleRate);

    SLresult result = EngineOpenSLES::getInstance().open();
    if (SL_RESULT_SUCCESS != result) {
        return Result::ErrorInternal;
    }

    Result oboeResult = AudioStreamBuffered::open();
    if (oboeResult != Result::OK) {
        return oboeResult;
    }
    // Convert to defaults if UNSPECIFIED
    if (mSampleRate == kUnspecified) {
        mSampleRate = DefaultStreamValues::SampleRate;
    }
    if (mChannelCount == kUnspecified) {
        mChannelCount = DefaultStreamValues::ChannelCount;
    }

    // Decide frames per burst based on hints from caller.
    // TODO  Can we query this from OpenSL ES?
    if (mFramesPerCallback != kUnspecified) {
        mFramesPerBurst = mFramesPerCallback;
    } else if (mFramesPerBurst != kUnspecified) { // set from defaultFramesPerBurst
        mFramesPerCallback = mFramesPerBurst;
    } else {
        mFramesPerBurst = mFramesPerCallback = DefaultStreamValues::FramesPerBurst;
    }

    mBytesPerCallback = mFramesPerCallback * getBytesPerFrame();
    LOGD("AudioStreamOpenSLES(): mFramesPerCallback = %d", mFramesPerCallback);
    LOGD("AudioStreamOpenSLES(): mBytesPerCallback = %d", mBytesPerCallback);
    if (mBytesPerCallback <= 0) {
        LOGE("AudioStreamOpenSLES::open() bytesPerCallback < 0, bad format?");
        return Result::ErrorInvalidFormat; // causing bytesPerFrame == 0
    }
    delete[] mCallbackBuffer; // to prevent memory leaks
    mCallbackBuffer = new uint8_t[mBytesPerCallback];

    mSharingMode = SharingMode::Shared;

    if (!usingFIFO()) {
        mBufferCapacityInFrames = mFramesPerBurst * kBufferQueueLength;
        mBufferSizeInFrames = mBufferCapacityInFrames;
    }

    return Result::OK;
}

SLuint32 AudioStreamOpenSLES::convertPerformanceMode(PerformanceMode oboeMode) const {
    SLuint32 openslMode = SL_ANDROID_PERFORMANCE_NONE;
    switch(oboeMode) {
        case PerformanceMode::None:
            openslMode =  SL_ANDROID_PERFORMANCE_NONE;
            break;
        case PerformanceMode::LowLatency:
            openslMode =  (getSessionId() == SessionId::None) ?  SL_ANDROID_PERFORMANCE_LATENCY : SL_ANDROID_PERFORMANCE_LATENCY_EFFECTS;
            break;
        case PerformanceMode::PowerSaving:
            openslMode =  SL_ANDROID_PERFORMANCE_POWER_SAVING;
            break;
        default:
            break;
    }
    return openslMode;
}

PerformanceMode AudioStreamOpenSLES::convertPerformanceMode(SLuint32 openslMode) const {
    PerformanceMode oboeMode = PerformanceMode::None;
    switch(openslMode) {
        case SL_ANDROID_PERFORMANCE_NONE:
            oboeMode =  PerformanceMode::None;
            break;
        case SL_ANDROID_PERFORMANCE_LATENCY:
        case SL_ANDROID_PERFORMANCE_LATENCY_EFFECTS:
            oboeMode =  PerformanceMode::LowLatency;
            break;
        case SL_ANDROID_PERFORMANCE_POWER_SAVING:
            oboeMode =  PerformanceMode::PowerSaving;
            break;
        default:
            break;
    }
    return oboeMode;
}

SLresult AudioStreamOpenSLES::configurePerformanceMode(SLAndroidConfigurationItf configItf) {

    if (configItf == nullptr) {
        LOGW("%s() called with NULL configuration", __func__);
        mPerformanceMode = PerformanceMode::None;
        return SL_RESULT_INTERNAL_ERROR;
    }
    if (getSdkVersion() < __ANDROID_API_N_MR1__) {
        LOGW("%s() not supported until N_MR1", __func__);
        mPerformanceMode = PerformanceMode::None;
        return SL_RESULT_SUCCESS;
    }

    SLresult result = SL_RESULT_SUCCESS;
    SLuint32 performanceMode = convertPerformanceMode(getPerformanceMode());
    LOGD("SetConfiguration(SL_ANDROID_KEY_PERFORMANCE_MODE, SL %u) called", performanceMode);
    result = (*configItf)->SetConfiguration(configItf, SL_ANDROID_KEY_PERFORMANCE_MODE,
                                                     &performanceMode, sizeof(performanceMode));
    if (SL_RESULT_SUCCESS != result) {
        LOGW("SetConfiguration(PERFORMANCE_MODE, SL %u) returned %s",
             performanceMode, getSLErrStr(result));
        mPerformanceMode = PerformanceMode::None;
    }

    return result;
}

SLresult AudioStreamOpenSLES::updateStreamParameters(SLAndroidConfigurationItf configItf) {
    SLresult result = SL_RESULT_SUCCESS;
    if(getSdkVersion() >= __ANDROID_API_N_MR1__ && configItf != nullptr) {
        SLuint32 performanceMode = 0;
        SLuint32 performanceModeSize = sizeof(performanceMode);
        result = (*configItf)->GetConfiguration(configItf, SL_ANDROID_KEY_PERFORMANCE_MODE,
                                                &performanceModeSize, &performanceMode);
        // A bug in GetConfiguration() before P caused a wrong result code to be returned.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            result = SL_RESULT_SUCCESS; // Ignore actual result before P.
        }

        if (SL_RESULT_SUCCESS != result) {
            LOGW("GetConfiguration(SL_ANDROID_KEY_PERFORMANCE_MODE) returned %d", result);
            mPerformanceMode = PerformanceMode::None; // If we can't query it then assume None.
        } else {
            mPerformanceMode = convertPerformanceMode(performanceMode); // convert SL to Oboe mode
        }
    } else {
        mPerformanceMode = PerformanceMode::None; // If we can't query it then assume None.
    }
    return result;
}

Result AudioStreamOpenSLES::close() {

    if (mState == StreamState::Closed){
        return Result::ErrorClosed;
    } else {
        AudioStreamBuffered::close();

        onBeforeDestroy();

        if (mObjectInterface != nullptr) {
            (*mObjectInterface)->Destroy(mObjectInterface);
            mObjectInterface = nullptr;

        }

        onAfterDestroy();

        mSimpleBufferQueueInterface = nullptr;
        EngineOpenSLES::getInstance().close();

        setState(StreamState::Closed);
        return Result::OK;
    }
}

SLresult AudioStreamOpenSLES::enqueueCallbackBuffer(SLAndroidSimpleBufferQueueItf bq) {
    return (*bq)->Enqueue(bq, mCallbackBuffer, mBytesPerCallback);
}

void AudioStreamOpenSLES::processBufferCallback(SLAndroidSimpleBufferQueueItf bq) {
    bool stopStream = false;
    // Ask the callback to fill the output buffer with data.
    DataCallbackResult result = fireDataCallback(mCallbackBuffer, mFramesPerCallback);
    if (result == DataCallbackResult::Continue) {
        // Update Oboe service position based on OpenSL ES position.
        updateServiceFrameCounter();
        // Update Oboe client position with frames handled by the callback.
        if (getDirection() == Direction::Input) {
            mFramesRead += mFramesPerCallback;
        } else {
            mFramesWritten += mFramesPerCallback;
        }
        // Pass the data to OpenSLES.
        SLresult enqueueResult = enqueueCallbackBuffer(bq);
        if (enqueueResult != SL_RESULT_SUCCESS) {
            LOGE("enqueueCallbackBuffer() returned %d", enqueueResult);
            stopStream = true;
        }
    } else if (result == DataCallbackResult::Stop) {
        LOGD("Oboe callback returned Stop");
        stopStream = true;
    } else {
        LOGW("Oboe callback returned unexpected value = %d", result);
        stopStream = true;
    }
    if (stopStream) {
        requestStop();
    }
}

// this callback handler is called every time a buffer needs processing
static void bqCallbackGlue(SLAndroidSimpleBufferQueueItf bq, void *context) {
    (reinterpret_cast<AudioStreamOpenSLES *>(context))->processBufferCallback(bq);
}

SLresult AudioStreamOpenSLES::registerBufferQueueCallback() {
    // The BufferQueue
    SLresult result = (*mObjectInterface)->GetInterface(mObjectInterface, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                &mSimpleBufferQueueInterface);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get buffer queue interface:%p result:%s",
             mSimpleBufferQueueInterface,
             getSLErrStr(result));
    } else {
        // Register the BufferQueue callback
        result = (*mSimpleBufferQueueInterface)->RegisterCallback(mSimpleBufferQueueInterface,
                                                                  bqCallbackGlue, this);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("RegisterCallback result:%s", getSLErrStr(result));
        }
    }
    return result;
}

int32_t AudioStreamOpenSLES::getFramesPerBurst() {
    return mFramesPerBurst;
}

int64_t AudioStreamOpenSLES::getFramesProcessedByServer() const {
    int64_t millis64 = mPositionMillis.get();
    int64_t framesProcessed = millis64 * getSampleRate() / kMillisPerSecond;
    return framesProcessed;
}

Result AudioStreamOpenSLES::waitForStateChange(StreamState currentState,
                                                     StreamState *nextState,
                                                     int64_t timeoutNanoseconds) {
    Result oboeResult = Result::ErrorTimeout;
    int64_t sleepTimeNanos = 20 * kNanosPerMillisecond; // arbitrary
    int64_t timeLeftNanos = timeoutNanoseconds;

    while (true) {
        const StreamState state = getState(); // this does not require a lock
        if (nextState != nullptr) {
            *nextState = state;
        }
        if (currentState != state) { // state changed?
            oboeResult = Result::OK;
            break;
        }

        // Did we timeout or did user ask for non-blocking?
        if (timeoutNanoseconds <= 0) {
            break;
        }

        if (sleepTimeNanos > timeLeftNanos){
            sleepTimeNanos = timeLeftNanos;
        }
        AudioClock::sleepForNanos(sleepTimeNanos);
        timeLeftNanos -= sleepTimeNanos;
    }

    return oboeResult;
}
