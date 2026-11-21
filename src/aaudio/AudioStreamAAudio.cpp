/*
 * Copyright 2016 The Android Open Source Project
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

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <pthread.h>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "aaudio/AAudioLoader.h"
#include "aaudio/AudioStreamAAudio.h"
#include "aaudio/AAudioStreamRegistry.h"
#include "common/OboeDebug.h"
#include "oboe/AudioClock.h"
#include "oboe/Utilities.h"
#include "AAudioExtensions.h"

#ifdef __ANDROID__
#include <sys/system_properties.h>
#include <common/QuirksManager.h>

#endif

#ifndef OBOE_FIX_FORCE_STARTING_TO_STARTED
// Workaround state problems in AAudio
// TODO Which versions does this occur in? Verify fixed in Q.
#define OBOE_FIX_FORCE_STARTING_TO_STARTED 1
#endif // OBOE_FIX_FORCE_STARTING_TO_STARTED

using namespace oboe;
AAudioLoader *AudioStreamAAudio::mLibLoader = nullptr;

namespace {
// Requests a stop, skipping it on O_MR1 and older if the stream is already stopping or stopped,
// because those versions return a state-machine error in that case.
Result requestStopChecked(AAudioLoader *libLoader, AAudioStream *stream) {
    if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
        StreamState state = static_cast<StreamState>(libLoader->stream_getState(stream));
        if (state == StreamState::Stopping || state == StreamState::Stopped) {
            return Result::OK;
        }
    }
    return static_cast<Result>(libLoader->stream_requestStop(stream));
}
} // namespace

namespace oboe {

int32_t AudioStreamAAudio::getSlotCountForTest() {
    return AAudioStreamRegistry::getInUseCountForTest();
}

int32_t AudioStreamAAudio::getAllocatedSlotCountForTest() {
    return AAudioStreamRegistry::getAllocatedCountForTest();
}

} // namespace oboe

// Real-time data callback wrappers. Must stay lock-free and allocation-free while returning
// Continue (no registry lock, mutexes, shared_ptr copies, thread_local, or logging).
static aaudio_data_callback_result_t oboe_aaudio_data_callback_proc(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames) {
    AAudioStreamRegistry::DataSlot *slot = nullptr;
    uint32_t expectedGeneration = 0;
    AudioStreamAAudio *oboeStream =
            AAudioStreamRegistry::enterDataCallback(userData, &slot, &expectedGeneration);
    const DataCallbackResult result = (oboeStream != nullptr)
            ? oboeStream->callOnAudioReady(stream, audioData, numFrames,
                                           slot->generation, expectedGeneration)
            : DataCallbackResult::Stop; // closed: never touch the stream
    AAudioStreamRegistry::leaveDataCallback(slot);
    return static_cast<aaudio_data_callback_result_t>(result);
}

static int32_t oboe_aaudio_partial_data_callback_proc(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames) {
    AAudioStreamRegistry::DataSlot *slot = nullptr;
    uint32_t expectedGeneration = 0;
    AudioStreamAAudio *oboeStream =
            AAudioStreamRegistry::enterDataCallback(userData, &slot, &expectedGeneration);
    const int32_t result = (oboeStream != nullptr)
            ? oboeStream->callOnPartialAudioReady(stream, audioData, numFrames,
                                                  slot->generation, expectedGeneration)
            : -1; // closed: negative value stops the stream
    AAudioStreamRegistry::leaveDataCallback(slot);
    return result;
}

// Runs `fn(oboeStream)` on a detached helper thread while holding `refs.lease` (for raw-pointer
// streams) and locked shared_ptrs (for shared_ptr-owned streams and FilterAudioStream parents).
template <typename Fn>
static void spawnHelperThread(AAudioStreamRegistry::CallbackRefs refs, Fn &&fn) {
    // Declare shared_ptrs before lease so lease is always destroyed first on unwind; otherwise
    // dropping the last shared_ptr on this thread would deadlock on our own unattached lease.
    std::shared_ptr<AudioStream> sharedStream;
    std::shared_ptr<AudioStream> sharedParentStream;
    if (!refs.lockSharedPtrs(sharedStream, sharedParentStream)) {
        return;
    }
    HelperLease lease = std::move(refs.lease);
    AudioStreamAAudio *oboeStream = refs.stream;
    std::thread([oboeStream,
                 sharedStream = std::move(sharedStream),
                 sharedParentStream = std::move(sharedParentStream),
                 lease = std::move(lease),
                 fn = std::forward<Fn>(fn)]() mutable {
        lease.attachToCurrentThread();
        fn(oboeStream);
        // oboeStream may have been deleted by fn (e.g. onErrorAfterClose()); do not touch it below.
        lease.reset();
        (void) sharedStream;
        (void) sharedParentStream;
    }).detach();
}

namespace oboe {

/*
 * Create a stream that uses Oboe Audio API.
 */
AudioStreamAAudio::AudioStreamAAudio(const AudioStreamBuilder &builder)
    : AudioStream(builder)
    , mAAudioStream(nullptr) {
    mCallbackThreadEnabled.store(false);
    mLibLoader = AAudioLoader::getInstance();
    mHelperThreads = std::make_shared<HelperThreadTracker>();
}

AudioStreamAAudio::~AudioStreamAAudio() {
    closeAndWaitForHelperThreads();
}

void AudioStreamAAudio::closeAndWaitForChild(AudioStream *child) {
    // FilterAudioStream's child is never itself a FilterAudioStream, so AudioApi::AAudio
    // uniquely identifies AudioStreamAAudio without RTTI.
    if (child != nullptr && child->getAudioApi() == AudioApi::AAudio) {
        static_cast<AudioStreamAAudio *>(child)->closeAndWaitForHelperThreads();
    }
}

namespace {
// Process that registered closeAllStreamsAtExit(); a forked child calling exit() must not close
// inherited parent streams.
std::atomic<pid_t> sExitHandlerPid{0};
} // namespace

void AudioStreamAAudio::closeAllStreamsAtExit() {
    if (getpid() != sExitHandlerPid.load()) {
        return;
    }
    AAudioStreamRegistry::drainForExit([](AudioStreamAAudio *stream) {
        if (!stream->isOnOwnDataCallbackThread()) {
            LOGW("AudioStreamAAudio(%p) still open at exit; closing stream now.", stream);
            stream->closeInternal(CloseMode::Exit);
        }
    });
}

void AudioStreamAAudio::registerExitHandler() {
    static const bool sRegistered = [] {
        sExitHandlerPid.store(getpid());
        return std::atexit(&AudioStreamAAudio::closeAllStreamsAtExit) == 0;
    }();
    (void) sRegistered;
}

void AudioStreamAAudio::onWeakThisSet(AudioStream *stream,
                                      const std::shared_ptr<oboe::AudioStream> &sharedStream) {
    AAudioStreamRegistry::onWeakThisSet(stream, sharedStream);
}

void AudioStreamAAudio::closeAndWaitForHelperThreads() {
    // Close first (draining data callbacks and deactivating control callbacks), then wait for
    // helper threads that already hold a lease before freeing memory.
    if (mAAudioStream.load() == nullptr) {
        // Another thread may still be inside closeInternal() after nulling mAAudioStream;
        // serialize on mCloseLock so teardown finishes before `this` is freed. Cannot lock from
        // the own data callback thread because AAudioStream_close() waits for that callback.
        if (!isOnOwnDataCallbackThread()) {
            std::lock_guard<std::mutex> closeLock(mCloseLock);
        }
    } else if (!isOnOwnDataCallbackThread()) {
        LOGW("AudioStreamAAudio(%p) destroyed without close()! Closing stream now.", this);
        closeInternal(CloseMode::Destructor);
    } else {
        // Unsupported: app released its last reference inside onAudioReady(). Do not block the RT
        // thread; unregister and invalidate the slot now, then stop/close AAudio asynchronously.
        AAudioStreamRegistry::deactivate(mSlotHandle.load(std::memory_order_acquire));
        AAudioStream *stream = mAAudioStream.exchange(nullptr);
        if (stream != nullptr) {
            AAudioStreamRegistry::release(mSlotHandle.exchange(0, std::memory_order_acq_rel),
                                          /*waitForCallbacks=*/false);
            closePerformanceHint();
            LOGW("AudioStreamAAudio(%p) destroyed from its own data callback without close(). "
                 "This is unsupported; closing the AAudioStream asynchronously.", this);
            AAudioLoader *libLoader = mLibLoader;
            const bool workarounds = OboeGlobals::areWorkaroundsEnabled();
            const int32_t delayMillis = std::min<int32_t>(mDelayBeforeCloseMillis,
                                                          kMaxDelayBeforeCloseMillis);
            std::thread([libLoader, stream, workarounds, delayMillis]() {
                requestStopChecked(libLoader, stream);
                if (workarounds && delayMillis > 0) {
                    usleep(static_cast<useconds_t>(delayMillis) * 1000);
                }
                libLoader->stream_close(stream);
            }).detach();
        }
    }
    if (mHelperThreads && !isOnOwnDataCallbackThread()) {
        mHelperThreads->waitForOtherThreads(this);
    }
}

bool AudioStreamAAudio::isSupported() {
    mLibLoader = AAudioLoader::getInstance();
    int openResult = mLibLoader->open();
    return openResult == 0;
}

// Static method for the error callback.
// Launches a helper thread that can safely stop, close and delete the stream.
void AudioStreamAAudio::internalErrorCallback(
        AAudioStream *stream,
        void *userData,
        aaudio_result_t error) {
    oboe::Result oboeResult = static_cast<Result>(error);

    // Coerce the error code if needed to workaround a regression in RQ1A that caused
    // the wrong code to be passed when headsets plugged in. See b/173928197.
    if (OboeGlobals::areWorkaroundsEnabled()
            && getSdkVersion() == __ANDROID_API_R__
            && oboeResult == oboe::Result::ErrorTimeout) {
        oboeResult = oboe::Result::ErrorDisconnected;
        LOGD("%s() ErrorTimeout changed to ErrorDisconnected to fix b/173928197", __func__);
    }

    auto refs = AAudioStreamRegistry::lookupControl(userData);
    if (!refs) {
        return;
    }

    AudioStreamAAudio *oboeStream = refs.stream;
    // Check non-virtual atomic mAAudioStream first so we never mutate state or dispatch
    // virtually on a stream whose close/destructor is already running.
    if (stream != oboeStream->mAAudioStream.load()) {
        LOGW("%s() stream already closed or closing", __func__);
    } else if (oboeStream->wasErrorCallbackCalled()) { // block extra error callbacks
        LOGE("%s() multiple error callbacks called!", __func__);
    } else {
        oboeStream->mErrorCallbackResult = oboeResult;
        spawnHelperThread(std::move(refs), [oboeResult](AudioStreamAAudio *s) {
            AudioStreamErrorCallback *errorCallback = s->getErrorCallback();
            if (errorCallback == nullptr) return;
            if (!errorCallback->onError(s, oboeResult)) {
                s->requestStop();
                errorCallback->onErrorBeforeClose(s, oboeResult);
                s->close();
                // Warning: s may get deleted by onErrorAfterClose().
                errorCallback->onErrorAfterClose(s, oboeResult);
            }
        });
    }
}

void AudioStreamAAudio::beginPerformanceHintInCallback() {
    if (isPerformanceHintEnabled()) {
        if (!mAdpfOpenAttempted) {
            int64_t targetDurationNanos = (mFramesPerBurst * 1e9) / getSampleRate();
            // This has to be called from the callback thread so we get the right TID.
            int adpfResult = mAdpfWrapper.open(gettid(), targetDurationNanos);
            if (adpfResult < 0) {
                LOGW("WARNING ADPF not supported, %d\n", adpfResult);
            } else {
                LOGD("ADPF is now active\n");
            }
            mAdpfOpenAttempted = true;
        }
        mAdpfWrapper.onBeginCallback();
    } else if (!isPerformanceHintEnabled() && mAdpfOpenAttempted) {
        LOGD("ADPF closed\n");
        mAdpfWrapper.close();
        mAdpfOpenAttempted = false;
    }
}

void AudioStreamAAudio::endPerformanceHintInCallback(int32_t numFrames) {
    if (mAdpfWrapper.isOpen()) {
        // Scale the measured duration based on numFrames so it is normalized to a full burst.
        double durationScaler = static_cast<double>(mFramesPerBurst) / numFrames;
        // Skip this callback if numFrames is very small.
        // This can happen when buffers wrap around, particularly when doing sample rate conversion.
        if (durationScaler < 2.0) {
            mAdpfWrapper.onEndCallback(durationScaler);
        }
    }
}

void AudioStreamAAudio::logUnsupportedAttributes() {
    int sdkVersion = getSdkVersion();

    // These attributes are not supported pre Android "P"
    if (sdkVersion < __ANDROID_API_P__) {
        if (mUsage != Usage::Media) {
            LOGW("Usage [AudioStreamBuilder::setUsage()] "
                 "is not supported on AAudio streams running on pre-Android P versions.");
        }

        if (mContentType != ContentType::Music) {
            LOGW("ContentType [AudioStreamBuilder::setContentType()] "
                 "is not supported on AAudio streams running on pre-Android P versions.");
        }

        if (mSessionId != SessionId::None) {
            LOGW("SessionId [AudioStreamBuilder::setSessionId()] "
                 "is not supported on AAudio streams running on pre-Android P versions.");
        }
    }
}

Result AudioStreamAAudio::open() {
    Result result = Result::OK;

    if (mAAudioStream != nullptr) {
        return Result::ErrorInvalidState;
    }

    result = AudioStream::open();
    if (result != Result::OK) {
        return result;
    }

    // Ambisonic channel masks were added in API 38. Never fall back to a positional
    // stream with the same channel count because the data would be misinterpreted.
    if (mChannelAcnMask != ChannelAcnMask::Unspecified
            && (mLibLoader->builder_setChannelAcnMask == nullptr
                    || mLibLoader->stream_getChannelAcnMask == nullptr)) {
        LOGE("AudioStreamAAudio.open() ambisonic channel mask is not supported");
        return Result::ErrorUnimplemented;
    }

    AAudioStreamBuilder *aaudioBuilder;
    result = static_cast<Result>(mLibLoader->createStreamBuilder(&aaudioBuilder));
    if (result != Result::OK) {
        return result;
    }
    uintptr_t slotHandle = 0;
    void *callbackUserData = nullptr;

    // Do not set INPUT capacity below 4096 because that prevents us from getting a FAST track
    // when using the Legacy data path.
    // If the app requests > 4096 then we allow it but we are less likely to get LowLatency.
    // See internal bug b/80308183 for more details.
    // Fixed in Q but let's still clip the capacity because high input capacity
    // does not increase latency.
    int32_t capacity = mBufferCapacityInFrames;
    constexpr int kCapacityRequiredForFastLegacyTrack = 4096; // matches value in AudioFinger
    if (OboeGlobals::areWorkaroundsEnabled()
            && mDirection == oboe::Direction::Input
            && capacity != oboe::Unspecified
            && capacity < kCapacityRequiredForFastLegacyTrack
            && mPerformanceMode == oboe::PerformanceMode::LowLatency) {
        capacity = kCapacityRequiredForFastLegacyTrack;
        LOGD("AudioStreamAAudio.open() capacity changed from %d to %d for lower latency",
             static_cast<int>(mBufferCapacityInFrames), capacity);
    }
    mLibLoader->builder_setBufferCapacityInFrames(aaudioBuilder, capacity);

    if (mLibLoader->builder_setSessionId != nullptr) {
        mLibLoader->builder_setSessionId(aaudioBuilder,
                                         static_cast<aaudio_session_id_t>(mSessionId));
        // Output effects do not support PerformanceMode::LowLatency.
        if (OboeGlobals::areWorkaroundsEnabled()
                && mSessionId != SessionId::None
                && mDirection == oboe::Direction::Output
                && mPerformanceMode == PerformanceMode::LowLatency) {
                    mPerformanceMode = PerformanceMode::None;
                    LOGD("AudioStreamAAudio.open() performance mode changed to None when session "
                         "id is requested");
        }
    }

    // Channel mask was added in SC_V2. Given the corresponding channel count of selected channel
    // mask may be different from selected channel count, the last set value will be respected.
    // If channel count is set after channel mask, the previously set channel mask will be cleared.
    // If channel mask is set after channel count, the channel count will be automatically
    // calculated from selected channel mask. In that case, only set channel mask when the API
    // is available and the channel mask is specified.
    // The ambisonic channel mask follows the same rule. It is only set when specified.
    if (mChannelAcnMask != ChannelAcnMask::Unspecified) {
        mLibLoader->builder_setChannelAcnMask(aaudioBuilder,
                                              static_cast<uint32_t>(mChannelAcnMask));
    } else if (mLibLoader->builder_setChannelMask != nullptr
            && mChannelMask != ChannelMask::Unspecified) {
        mLibLoader->builder_setChannelMask(aaudioBuilder,
                                           static_cast<aaudio_channel_mask_t>(mChannelMask));
    } else {
        mLibLoader->builder_setChannelCount(aaudioBuilder, mChannelCount);
    }
    mLibLoader->builder_setDeviceId(aaudioBuilder, AudioStreamBase::getDeviceId());
    mLibLoader->builder_setDirection(aaudioBuilder, static_cast<aaudio_direction_t>(mDirection));
    mLibLoader->builder_setFormat(aaudioBuilder, static_cast<aaudio_format_t>(mFormat));
    mLibLoader->builder_setSampleRate(aaudioBuilder, mSampleRate);
    mLibLoader->builder_setSharingMode(aaudioBuilder,
                                       static_cast<aaudio_sharing_mode_t>(mSharingMode));
    mLibLoader->builder_setPerformanceMode(aaudioBuilder,
                                           static_cast<aaudio_performance_mode_t>(mPerformanceMode));

    // These were added in P so we have to check for the function pointer.
    if (mLibLoader->builder_setUsage != nullptr) {
        mLibLoader->builder_setUsage(aaudioBuilder,
                                     static_cast<aaudio_usage_t>(mUsage));
    }

    if (mLibLoader->builder_setContentType != nullptr) {
        mLibLoader->builder_setContentType(aaudioBuilder,
                                           static_cast<aaudio_content_type_t>(mContentType));
    }

    if (mLibLoader->builder_setInputPreset != nullptr) {
        aaudio_input_preset_t inputPreset = mInputPreset;
        if (getSdkVersion() <= __ANDROID_API_P__ && inputPreset == InputPreset::VoicePerformance) {
            LOGD("InputPreset::VoicePerformance not supported before Q. Using VoiceRecognition.");
            inputPreset = InputPreset::VoiceRecognition; // most similar preset
        }
        mLibLoader->builder_setInputPreset(aaudioBuilder,
                                           static_cast<aaudio_input_preset_t>(inputPreset));
    }

    // These were added in S so we have to check for the function pointer.
    if (mLibLoader->builder_setPackageName != nullptr && !mPackageName.empty()) {
        mLibLoader->builder_setPackageName(aaudioBuilder,
                                           mPackageName.c_str());
    }

    if (mLibLoader->builder_setAttributionTag != nullptr && !mAttributionTag.empty()) {
        mLibLoader->builder_setAttributionTag(aaudioBuilder,
                                           mAttributionTag.c_str());
    }

    // This was added in Q so we have to check for the function pointer.
    if (mLibLoader->builder_setAllowedCapturePolicy != nullptr && mDirection == oboe::Direction::Output) {
        mLibLoader->builder_setAllowedCapturePolicy(aaudioBuilder,
                                           static_cast<aaudio_allowed_capture_policy_t>(mAllowedCapturePolicy));
    }

    if (mLibLoader->builder_setPrivacySensitive != nullptr && mDirection == oboe::Direction::Input
            && mPrivacySensitiveMode != PrivacySensitiveMode::Unspecified) {
        mLibLoader->builder_setPrivacySensitive(aaudioBuilder,
                mPrivacySensitiveMode == PrivacySensitiveMode::Enabled);
    }

    if (mLibLoader->builder_setIsContentSpatialized != nullptr) {
        mLibLoader->builder_setIsContentSpatialized(aaudioBuilder, mIsContentSpatialized);
    }

    if (mSpatializationBehavior == SpatializationBehavior::Unspecified) {
        mSpatializationBehavior = SpatializationBehavior::Auto;
    }
    if (mLibLoader->builder_setSpatializationBehavior != nullptr) {
        mLibLoader->builder_setSpatializationBehavior(aaudioBuilder,
                static_cast<aaudio_spatialization_behavior_t>(mSpatializationBehavior));
    } else {
        mSpatializationBehavior = SpatializationBehavior::Auto;
    }

    if (!AAudioStreamRegistry::acquire(this, &slotHandle)) {
        LOGE("AudioStreamAAudio.open() too many streams open at once");
        result = Result::ErrorNoFreeHandles;
        goto error2;
    }
    mSlotHandle.store(slotHandle, std::memory_order_release);
    callbackUserData = reinterpret_cast<void *>(slotHandle);

    if (anyDataCallbackSpecified()) {
        if (isDataCallbackSpecified()) {
            mLibLoader->builder_setDataCallback(
                    aaudioBuilder, oboe_aaudio_data_callback_proc, callbackUserData);
        } else if (isPartialDataCallbackSpecified()) {
            if (mLibLoader->builder_setPartialDataCallback == nullptr) {
                LOGE("Using partial data callback while it is not available");
                result = Result::ErrorIllegalArgument;
                goto error2;
            }
            mLibLoader->builder_setPartialDataCallback(
                    aaudioBuilder, oboe_aaudio_partial_data_callback_proc, callbackUserData);
        }
        mLibLoader->builder_setFramesPerDataCallback(aaudioBuilder, getFramesPerDataCallback());

        if (!isErrorCallbackSpecified()) {
            // The app did not specify a callback so we should specify
            // our own so the stream gets closed and stopped.
            mErrorCallback = &mDefaultErrorCallback;
        }
        mLibLoader->builder_setErrorCallback(aaudioBuilder, internalErrorCallback,
                                             callbackUserData);
    }
    // Else if the data callback is not being used then the write method will return an error
    // and the app can stop and close the stream.

    if (isPresentationCallbackSpecified() &&
        mLibLoader->builder_setPresentationEndCallback != nullptr) {
        mLibLoader->builder_setPresentationEndCallback(aaudioBuilder,
                                                       internalPresentationEndCallback,
                                                       callbackUserData);
    }

    if (mLibLoader->builder_setRoutingChangedCallback != nullptr) {
        mLibLoader->builder_setRoutingChangedCallback(aaudioBuilder,
                                                      internalRoutingChangedCallback,
                                                      callbackUserData);
    }

    // ============= OPEN THE STREAM ================
    {
        AAudioStream *stream = nullptr;
        result = static_cast<Result>(mLibLoader->builder_openStream(aaudioBuilder, &stream));
        mAAudioStream.store(stream);
    }
    if (result != Result::OK) {
        // Warn developer because ErrorInternal is not very informative.
        if (result == Result::ErrorInternal && mDirection == Direction::Input) {
            LOGW("AudioStreamAAudio.open() may have failed due to lack of "
                 "audio recording permission.");
        }
        goto error2;
    }

    // Query and cache the stream properties
    mChannelCount = mLibLoader->stream_getChannelCount(mAAudioStream);
    mSampleRate = mLibLoader->stream_getSampleRate(mAAudioStream);
    mFormat = static_cast<AudioFormat>(mLibLoader->stream_getFormat(mAAudioStream));
    mSharingMode = static_cast<SharingMode>(mLibLoader->stream_getSharingMode(mAAudioStream));
    mPerformanceMode = static_cast<PerformanceMode>(
            mLibLoader->stream_getPerformanceMode(mAAudioStream));
    mBufferCapacityInFrames = mLibLoader->stream_getBufferCapacity(mAAudioStream);
    mBufferSizeInFrames = mLibLoader->stream_getBufferSize(mAAudioStream);
    mFramesPerBurst = mLibLoader->stream_getFramesPerBurst(mAAudioStream);

    // These were added in P so we have to check for the function pointer.
    if (mLibLoader->stream_getUsage != nullptr) {
        mUsage = static_cast<Usage>(mLibLoader->stream_getUsage(mAAudioStream));
    }
    if (mLibLoader->stream_getContentType != nullptr) {
        mContentType = static_cast<ContentType>(mLibLoader->stream_getContentType(mAAudioStream));
    }
    if (mLibLoader->stream_getInputPreset != nullptr) {
        mInputPreset = static_cast<InputPreset>(mLibLoader->stream_getInputPreset(mAAudioStream));
    }
    if (mLibLoader->stream_getSessionId != nullptr) {
        mSessionId = static_cast<SessionId>(mLibLoader->stream_getSessionId(mAAudioStream));
    } else {
        mSessionId = SessionId::None;
    }

    // This was added in Q so we have to check for the function pointer.
    if (mLibLoader->stream_getAllowedCapturePolicy != nullptr && mDirection == oboe::Direction::Output) {
        mAllowedCapturePolicy = static_cast<AllowedCapturePolicy>(mLibLoader->stream_getAllowedCapturePolicy(mAAudioStream));
    } else {
        mAllowedCapturePolicy = AllowedCapturePolicy::Unspecified;
    }

    if (mLibLoader->stream_isPrivacySensitive != nullptr && mDirection == oboe::Direction::Input) {
        bool isPrivacySensitive = mLibLoader->stream_isPrivacySensitive(mAAudioStream);
        mPrivacySensitiveMode = isPrivacySensitive ? PrivacySensitiveMode::Enabled :
                PrivacySensitiveMode::Disabled;
    } else {
        mPrivacySensitiveMode = PrivacySensitiveMode::Unspecified;
    }

    if (mLibLoader->stream_getChannelMask != nullptr) {
        mChannelMask = static_cast<ChannelMask>(mLibLoader->stream_getChannelMask(mAAudioStream));
    }

    if (mLibLoader->stream_getChannelAcnMask != nullptr) {
        mChannelAcnMask = static_cast<ChannelAcnMask>(
                mLibLoader->stream_getChannelAcnMask(mAAudioStream));
    } else {
        mChannelAcnMask = ChannelAcnMask::Unspecified;
    }

    if (mLibLoader->stream_isContentSpatialized != nullptr) {
        mIsContentSpatialized = mLibLoader->stream_isContentSpatialized(mAAudioStream);
    }

    if (mLibLoader->stream_getSpatializationBehavior != nullptr) {
        mSpatializationBehavior = static_cast<SpatializationBehavior>(
                mLibLoader->stream_getSpatializationBehavior(mAAudioStream));
    }

    if (mLibLoader->stream_getHardwareChannelCount != nullptr) {
        mHardwareChannelCount = mLibLoader->stream_getHardwareChannelCount(mAAudioStream);
    }
    if (mLibLoader->stream_getHardwareSampleRate != nullptr) {
        mHardwareSampleRate = mLibLoader->stream_getHardwareSampleRate(mAAudioStream);
    }
    if (mLibLoader->stream_getHardwareFormat != nullptr) {
        mHardwareFormat = static_cast<AudioFormat>(mLibLoader->stream_getHardwareFormat(mAAudioStream));
    }

    updateDeviceIds();

    LOGD("AudioStreamAAudio.open() format=%d, sampleRate=%d, capacity = %d",
            static_cast<int>(mFormat), static_cast<int>(mSampleRate),
            static_cast<int>(mBufferCapacityInFrames));

    calculateDefaultDelayBeforeCloseMillis();

error2:
    mLibLoader->builder_delete(aaudioBuilder);
    if (static_cast<int>(result) > 0) {
        // Possibly due to b/267531411
        LOGW("AudioStreamAAudio.open: AAudioStream_Open() returned positive error = %d",
             static_cast<int>(result));
        if (OboeGlobals::areWorkaroundsEnabled()) {
            result = Result::ErrorInternal; // Coerce to negative error.
        }
    } else {
        LOGD("AudioStreamAAudio.open: AAudioStream_Open() returned %s = %d",
             mLibLoader->convertResultToText(static_cast<aaudio_result_t>(result)),
             static_cast<int>(result));
        if (result == Result::OK) {
            std::shared_ptr<AudioStream> selfShared = lockWeakThis();
            std::shared_ptr<AudioStream> parentShared =
                    hasParentStream() ? getParentStream()->lockWeakThis() : nullptr;
            AAudioStreamRegistry::activate(slotHandle, this, mHelperThreads,
                                           selfShared, parentShared);
        }
    }
    if (result != Result::OK && slotHandle != 0) {
        mSlotHandle.store(0, std::memory_order_release);
        AAudioStreamRegistry::release(slotHandle, /*waitForCallbacks=*/false);
    }
    return result;
}

Result AudioStreamAAudio::release() {
    if (getSdkVersion() < __ANDROID_API_R__) {
        return Result::ErrorUnimplemented;
    }

    // AAudioStream_release() is buggy on Android R.
    if (OboeGlobals::areWorkaroundsEnabled() && getSdkVersion() == __ANDROID_API_R__) {
        LOGW("Skipping release() on Android R");
        return Result::ErrorUnimplemented;
    }

    // Not allowed from the stream's own data callback (see close()). Reject it, but make the
    // callback return DataCallbackResult::Stop so the audio still stops.
    if (isOnOwnDataCallbackThread()) {
        mStopRequestedFromCallback.store(true, std::memory_order_relaxed);
        LOGE("%s() called from the stream's own data callback; rejected. Return "
             "DataCallbackResult::Stop and release from another thread.", __func__);
        return Result::ErrorInvalidState;
    }

    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        if (OboeGlobals::areWorkaroundsEnabled()) {
            // Make sure we are really stopped. Do it under mLock
            // so another thread cannot call requestStart() right before the close.
            requestStop_l(stream);
        }
        return static_cast<Result>(mLibLoader->stream_release(stream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::close() {
    LOGD("%s", __func__);
    // Closing a stream from its own data callback is not allowed: AAudio rejects it, and it
    // can deadlock on older Android versions. Reject it without closing or unregistering
    // anything, but make the callback return DataCallbackResult::Stop so the audio stops.
    if (isOnOwnDataCallbackThread()) {
        mStopRequestedFromCallback.store(true, std::memory_order_relaxed);
        LOGE("%s() called from the stream's own data callback; rejected. Return "
             "DataCallbackResult::Stop and close from another thread.", __func__);
        return Result::ErrorInvalidState;
    }
    return closeInternal(CloseMode::App);
}

Result AudioStreamAAudio::closeInternal(CloseMode mode) {
    // Phase 1: Stop new error/routing/presentation/stop callbacks before closing.
    AAudioStreamRegistry::deactivate(mSlotHandle.load(std::memory_order_acquire));

    // Serializes concurrent close() calls and destructor cleanup; never taken by own data callback.
    std::lock_guard<std::mutex> closeLock(mCloseLock);

    AAudioStream *stream = nullptr;
    uintptr_t slotHandle = 0;
    {
        // Detach AAudioStream under mLock/mAAudioStreamLock so no other stream method can reach it,
        // then release both locks before AAudioStream_close() waits for running callbacks.
        std::lock_guard<std::mutex> lock(mLock);
        AudioStream::close();
        std::unique_lock<std::shared_mutex> lock2(mAAudioStreamLock);
        stream = mAAudioStream.exchange(nullptr);
        if (stream != nullptr) {
            slotHandle = mSlotHandle.exchange(0, std::memory_order_acq_rel);
        }
    }
    if (stream == nullptr) {
        return Result::ErrorClosed;
    }
    // Always stop before closing (avoids Android Q Legacy ~AudioTrack() hang on running streams).
    bool shouldDelay = (mode == CloseMode::App);
    if (mode == CloseMode::Destructor && OboeGlobals::areWorkaroundsEnabled()) {
        const auto stateBeforeStop = static_cast<StreamState>(mLibLoader->stream_getState(stream));
        shouldDelay = !(stateBeforeStop == StreamState::Open
                || stateBeforeStop == StreamState::Stopped
                || stateBeforeStop == StreamState::Paused
                || stateBeforeStop == StreamState::Flushed);
    }
    requestStopChecked(mLibLoader, stream);
    if (OboeGlobals::areWorkaroundsEnabled() && shouldDelay) {
        sleepBeforeClose();
    }
    const Result result = static_cast<Result>(mLibLoader->stream_close(stream));
    // Phase 2: Invalidate the slot and wait for any trailing Legacy data callback.
    AAudioStreamRegistry::release(slotHandle, /*waitForCallbacks=*/true);
    if (slotHandle != 0) {
        // Clear in case an in-flight callback exited early on generation mismatch.
        mDataCallbackThread.store(0, std::memory_order_relaxed);
    }
    // Close again in case a data callback re-opened ADPF after AudioStream::close() above.
    closePerformanceHint();
    return result;
}

Result AudioStreamAAudio::requestStopFromStopThread(uint32_t startGeneration) {
    std::lock_guard<std::mutex> lock(mLock);
    if (mStartGeneration.load(std::memory_order_relaxed) != startGeneration) {
        return Result::OK; // A newer requestStart() wins over this deferred stop.
    }
    AAudioStream *stream = mAAudioStream.load();
    return (stream != nullptr) ? requestStop_l(stream) : Result::ErrorClosed;
}

// Android R and older workaround (#1230): stops the stream on a helper thread when the data
// callback returns Stop. Never runs on the Continue hot path.
void AudioStreamAAudio::launchStopThread() {
    // Snapshot mStartGeneration before closing the gate so a concurrent requestStart() cannot
    // race ahead of our generation value.
    const uint32_t startGeneration = mStartGeneration.load(std::memory_order_acquire);
    if (!mStopThreadAllowed.exchange(false)) {
        return;
    }
    auto refs = AAudioStreamRegistry::lookupControl(
            reinterpret_cast<void *>(mSlotHandle.load(std::memory_order_relaxed)));
    if (!refs) {
        return;
    }
    spawnHelperThread(std::move(refs), [startGeneration](AudioStreamAAudio *s) {
        s->requestStopFromStopThread(startGeneration);
    });
}

DataCallbackResult AudioStreamAAudio::callOnAudioReady(
        AAudioStream * /*stream*/,
        void *audioData,
        int32_t numFrames,
        const std::atomic<uint32_t> &slotGeneration,
        uint32_t expectedGeneration) {
    DataCallbackResult result;
    if (!isDataCallbackEnabled()) {
        LOGW("AudioStreamAAudio::%s() called with data callback disabled!", __func__);
        result = DataCallbackResult::Stop;
    } else {
        // Lock-free marker so requestStop()/stop()/close()/release() and the destructor can tell
        // they were called from inside this callback (see isOnOwnDataCallbackThread()).
        mDataCallbackThread.store(static_cast<uintptr_t>(pthread_self()),
                                  std::memory_order_relaxed);
        beginPerformanceHintInCallback();

        // Invoke the app callback directly (instead of AudioStream::fireDataCallback()) so we can
        // check whether the callback destroyed `this` before touching any member or virtual method.
        if (mDataCallback) {
            result = mDataCallback->onAudioReady(this, audioData, numFrames);
        } else {
            result = onDefaultCallback(audioData, numFrames);
        }
        if (slotGeneration.load(std::memory_order_relaxed) != expectedGeneration) {
            // Either the app released its last reference inside onAudioReady(), or
            // closeInternal() already invalidated this slot (and will clear mDataCallbackThread
            // after leaveDataCallback() returns). Do not touch `this` below.
            return DataCallbackResult::Stop;
        }
        // On Oreo, we might get called after returning stop. So block that here.
        setDataCallbackEnabled(result == DataCallbackResult::Continue);
        endPerformanceHintInCallback(numFrames);

        if (mStopRequestedFromCallback.load(std::memory_order_relaxed)) {
            setDataCallbackEnabled(false);
            result = DataCallbackResult::Stop;
        }
    }
    if (result != DataCallbackResult::Continue) {
        if (result == DataCallbackResult::Stop) {
            LOGD("Oboe callback returned DataCallbackResult::Stop");
        } else {
            LOGE("Oboe callback returned unexpected value. Error: %d", static_cast<int>(result));
        }

        // Returning Stop caused various problems before S. See #1230
        if (OboeGlobals::areWorkaroundsEnabled() && getSdkVersion() <= __ANDROID_API_R__) {
            launchStopThread();
            result = DataCallbackResult::Continue;
        } else {
            result = DataCallbackResult::Stop; // OK >= API_S
        }
    }
    mDataCallbackThread.store(0, std::memory_order_relaxed);
    return result;
}

int32_t AudioStreamAAudio::callOnPartialAudioReady(
        AAudioStream * /*stream*/,
        void *audioData,
        int32_t numFrames,
        const std::atomic<uint32_t> &slotGeneration,
        uint32_t expectedGeneration) {
    if (!isDataCallbackEnabled()) {
        LOGW("AudioStreamAAudio::%s() called with data callback disabled!", __func__);
        return -1;
    }
    mDataCallbackThread.store(static_cast<uintptr_t>(pthread_self()), std::memory_order_relaxed);
    beginPerformanceHintInCallback();

    int32_t result;
    if (mPartialDataCallback) {
        result = mPartialDataCallback->onPartialAudioReady(this, audioData, numFrames);
    } else {
        LOGE("AudioStreamAAudio::%s, called without a partial data callback!", __func__);
        result = -1; // Negative value stops the stream.
    }
    if (slotGeneration.load(std::memory_order_relaxed) != expectedGeneration) {
        return -1;
    }
    if (result < 0) {
        setDataCallbackEnabled(false);
    }
    endPerformanceHintInCallback(numFrames);

    if (mStopRequestedFromCallback.load(std::memory_order_relaxed)) {
        // The app requested a stop from this callback (see callOnAudioReady()).
        setDataCallbackEnabled(false);
        result = -1; // A negative result stops the stream.
    }
    mDataCallbackThread.store(0, std::memory_order_relaxed);
    return result;
}

bool AudioStreamAAudio::isControlCallFromOwnCallbackRejected() const {
    return isOnOwnDataCallbackThread() && getSdkVersion() >= __ANDROID_API_P__;
}

Result AudioStreamAAudio::requestStart() {
    if (isControlCallFromOwnCallbackRejected()) {
        return Result::ErrorInvalidState;
    }
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        // Cancel any stop that the data callback requested but that has not happened yet
        // (see requestStopFromStopThread()), and re-enable the app's callback. Done before the
        // O_MR1 early return below, so a start always wins over an earlier pending stop.
        mStartGeneration.fetch_add(1, std::memory_order_release);
        if (anyDataCallbackSpecified()) {
            setDataCallbackEnabled(true);
        }
        mStopThreadAllowed = true;
        mStopRequestedFromCallback.store(false, std::memory_order_relaxed);
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(stream));
            if (state == StreamState::Starting || state == StreamState::Started) {
                // WARNING: On P, AAudio is returning ErrorInvalidState for Output and OK for Input.
                return Result::OK;
            }
        }
        closePerformanceHint();
        return static_cast<Result>(mLibLoader->stream_requestStart(stream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestPause() {
    if (isControlCallFromOwnCallbackRejected()) {
        return Result::ErrorInvalidState;
    }
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(stream));
            if (state == StreamState::Pausing || state == StreamState::Paused) {
                return Result::OK;
            }
        }
        return static_cast<Result>(mLibLoader->stream_requestPause(stream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestFlush() {
    if (isControlCallFromOwnCallbackRejected()) {
        return Result::ErrorInvalidState;
    }
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        // Avoid state machine errors in O_MR1.
        if (getSdkVersion() <= __ANDROID_API_O_MR1__) {
            StreamState state = static_cast<StreamState>(mLibLoader->stream_getState(stream));
            if (state == StreamState::Flushing || state == StreamState::Flushed) {
                return Result::OK;
            }
        }
        return static_cast<Result>(mLibLoader->stream_requestFlush(stream));
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::requestStop() {
    if (isOnOwnDataCallbackThread()) {
        // Called from this stream's own data callback. Keep the real-time thread lock-free:
        // make the callback return DataCallbackResult::Stop instead of taking mLock.
        // Returns OK immediately; the state changes only after the callback returns.
        mStopRequestedFromCallback.store(true, std::memory_order_relaxed);
        return Result::OK;
    }
    std::lock_guard<std::mutex> lock(mLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return requestStop_l(stream);
    } else {
        return Result::ErrorClosed;
    }
}

Result AudioStreamAAudio::stop(int64_t timeoutNanoseconds) {
    if (isOnOwnDataCallbackThread()) {
        // Called from this stream's own data callback. The stream cannot reach Stopped until
        // the callback returns, so waiting would only stall the real-time thread until the
        // timeout. Request the stop and return OK; the state changes after the callback returns.
        return requestStop();
    }
    return AudioStream::stop(timeoutNanoseconds);
}

// Call under mLock
Result AudioStreamAAudio::requestStop_l(AAudioStream *stream) {
    return requestStopChecked(mLibLoader, stream);
}

ResultWithValue<int32_t>   AudioStreamAAudio::write(const void *buffer,
                                     int32_t numFrames,
                                     int64_t timeoutNanoseconds) {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        int32_t result = mLibLoader->stream_write(mAAudioStream, buffer,
                                                  numFrames, timeoutNanoseconds);
        return ResultWithValue<int32_t>::createBasedOnSign(result);
    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}

ResultWithValue<int32_t>   AudioStreamAAudio::read(void *buffer,
                                 int32_t numFrames,
                                 int64_t timeoutNanoseconds) {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        int32_t result = mLibLoader->stream_read(mAAudioStream, buffer,
                                                 numFrames, timeoutNanoseconds);
        return ResultWithValue<int32_t>::createBasedOnSign(result);
    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}


// AAudioStream_waitForStateChange() can crash if it is waiting on a stream and that stream
// is closed from another thread.  We do not want to lock the stream for the duration of the call.
// So we call AAudioStream_waitForStateChange() with a timeout of zero so that it will not block.
// Then we can do our own sleep with the lock unlocked.
Result AudioStreamAAudio::waitForStateChange(StreamState currentState,
                                        StreamState *nextState,
                                        int64_t timeoutNanoseconds) {
    Result oboeResult = Result::ErrorTimeout;
    int64_t sleepTimeNanos = 20 * kNanosPerMillisecond; // arbitrary
    aaudio_stream_state_t currentAAudioState = static_cast<aaudio_stream_state_t>(currentState);

    aaudio_result_t result = AAUDIO_OK;
    int64_t timeLeftNanos = timeoutNanoseconds;

    mLock.lock();
    while (true) {
        // Do we still have an AAudio stream? If not then stream must have been closed.
        AAudioStream *stream = mAAudioStream.load();
        if (stream == nullptr) {
            if (nextState != nullptr) {
                *nextState = StreamState::Closed;
            }
            oboeResult = Result::ErrorClosed;
            break;
        }

        // Update and query state change with no blocking.
        aaudio_stream_state_t aaudioNextState;
        result = mLibLoader->stream_waitForStateChange(
                mAAudioStream,
                currentAAudioState,
                &aaudioNextState,
                0); // timeout=0 for non-blocking
        // AAudio will return AAUDIO_ERROR_TIMEOUT if timeout=0 and the state does not change.
        if (result != AAUDIO_OK && result != AAUDIO_ERROR_TIMEOUT) {
            oboeResult = static_cast<Result>(result);
            break;
        }
#if OBOE_FIX_FORCE_STARTING_TO_STARTED
        if (OboeGlobals::areWorkaroundsEnabled()
            && aaudioNextState == static_cast<aaudio_stream_state_t >(StreamState::Starting)) {
            aaudioNextState = static_cast<aaudio_stream_state_t >(StreamState::Started);
        }
#endif // OBOE_FIX_FORCE_STARTING_TO_STARTED
        if (nextState != nullptr) {
            *nextState = static_cast<StreamState>(aaudioNextState);
        }
        if (currentAAudioState != aaudioNextState) { // state changed?
            oboeResult = Result::OK;
            break;
        }

        // Did we timeout or did user ask for non-blocking?
        if (timeLeftNanos <= 0) {
            break;
        }

        // No change yet so sleep.
        mLock.unlock(); // Don't sleep while locked.
        if (sleepTimeNanos > timeLeftNanos) {
            sleepTimeNanos = timeLeftNanos; // last little bit
        }
        AudioClock::sleepForNanos(sleepTimeNanos);
        timeLeftNanos -= sleepTimeNanos;
        mLock.lock();
    }

    mLock.unlock();
    return oboeResult;
}

ResultWithValue<int32_t> AudioStreamAAudio::setBufferSizeInFrames(int32_t requestedFrames) {
    int32_t adjustedFrames = requestedFrames;
    if (adjustedFrames > mBufferCapacityInFrames) {
        adjustedFrames = mBufferCapacityInFrames;
    }
    // This calls getBufferSize() so avoid recursive lock.
    adjustedFrames = QuirksManager::getInstance().clipBufferSize(*this, adjustedFrames);

    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        int32_t newBufferSize = mLibLoader->stream_setBufferSize(mAAudioStream, adjustedFrames);
        // Cache the result if it's valid
        if (newBufferSize > 0) mBufferSizeInFrames = newBufferSize;
        return ResultWithValue<int32_t>::createBasedOnSign(newBufferSize);
    } else {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
}

StreamState AudioStreamAAudio::getState() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        aaudio_stream_state_t aaudioState = mLibLoader->stream_getState(stream);
#if OBOE_FIX_FORCE_STARTING_TO_STARTED
        if (OboeGlobals::areWorkaroundsEnabled()
            && aaudioState == AAUDIO_STREAM_STATE_STARTING) {
            aaudioState = AAUDIO_STREAM_STATE_STARTED;
        }
#endif // OBOE_FIX_FORCE_STARTING_TO_STARTED
        return static_cast<StreamState>(aaudioState);
    } else {
        return StreamState::Closed;
    }
}

void AudioStreamAAudio::onRoutingChanged(std::vector<int32_t> deviceIds) {
    const int32_t primaryId = deviceIds.empty() ? kUnspecified : deviceIds[0];
    std::lock_guard<std::mutex> lock(mUpdatedDeviceIds.lock);
    mUpdatedDeviceIds.deviceIds = std::move(deviceIds);
    mUpdatedDeviceIds.primaryId.store(primaryId, std::memory_order_relaxed);
}

int32_t AudioStreamAAudio::getDeviceId() const {
    return mUpdatedDeviceIds.primaryId.load(std::memory_order_relaxed);
}

std::vector<int32_t> AudioStreamAAudio::getDeviceIds() const {
    std::lock_guard<std::mutex> lock(mUpdatedDeviceIds.lock);
    return mUpdatedDeviceIds.deviceIds;
}

int32_t AudioStreamAAudio::getBufferSizeInFrames() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        mBufferSizeInFrames = mLibLoader->stream_getBufferSize(stream);
    }
    return mBufferSizeInFrames;
}

void AudioStreamAAudio::updateFramesRead() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
// Set to 1 for debugging race condition #1180 with mAAudioStream.
// See also DEBUG_CLOSE_RACE in OboeTester.
// This was left in the code so that we could test the fix again easily in the future.
// We could not trigger the race condition without adding these get calls and the sleeps.
#define DEBUG_CLOSE_RACE 0
#if DEBUG_CLOSE_RACE
    // This is used when testing race conditions with close().
    // See DEBUG_CLOSE_RACE in OboeTester
    AudioClock::sleepForNanos(400 * kNanosPerMillisecond);
#endif // DEBUG_CLOSE_RACE
    if (stream != nullptr) {
        mFramesRead = mLibLoader->stream_getFramesRead(stream);
    }
}

void AudioStreamAAudio::updateFramesWritten() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        mFramesWritten = mLibLoader->stream_getFramesWritten(stream);
    }
}

ResultWithValue<int32_t> AudioStreamAAudio::getXRunCount() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return ResultWithValue<int32_t>::createBasedOnSign(mLibLoader->stream_getXRunCount(stream));
    } else {
        return ResultWithValue<int32_t>(Result::ErrorNull);
    }
}

Result AudioStreamAAudio::getTimestamp(clockid_t clockId,
                                   int64_t *framePosition,
                                   int64_t *timeNanoseconds) {
    if (getState() != StreamState::Started) {
        return Result::ErrorInvalidState;
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return static_cast<Result>(mLibLoader->stream_getTimestamp(stream, clockId,
                                               framePosition, timeNanoseconds));
    } else {
        return Result::ErrorNull;
    }
}

ResultWithValue<double> AudioStreamAAudio::calculateLatencyMillis() {
    // Get the time that a known audio frame was presented.
    int64_t hardwareFrameIndex;
    int64_t hardwareFrameHardwareTime;
    auto result = getTimestamp(CLOCK_MONOTONIC,
                               &hardwareFrameIndex,
                               &hardwareFrameHardwareTime);
    if (result != oboe::Result::OK) {
        return ResultWithValue<double>(static_cast<Result>(result));
    }

    // Get counter closest to the app.
    bool isOutput = (getDirection() == oboe::Direction::Output);
    int64_t appFrameIndex = isOutput ? getFramesWritten() : getFramesRead();

    // Assume that the next frame will be processed at the current time
    using namespace std::chrono;
    int64_t appFrameAppTime =
            duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();

    // Calculate the number of frames between app and hardware
    int64_t frameIndexDelta = appFrameIndex - hardwareFrameIndex;

    // Calculate the time which the next frame will be or was presented
    int64_t frameTimeDelta = (frameIndexDelta * oboe::kNanosPerSecond) / getSampleRate();
    int64_t appFrameHardwareTime = hardwareFrameHardwareTime + frameTimeDelta;

    // The current latency is the difference in time between when the current frame is at
    // the app and when it is at the hardware.
    double latencyNanos = static_cast<double>(isOutput
                          ? (appFrameHardwareTime - appFrameAppTime) // hardware is later
                          : (appFrameAppTime - appFrameHardwareTime)); // hardware is earlier
    double latencyMillis = latencyNanos / kNanosPerMillisecond;

    return ResultWithValue<double>(latencyMillis);
}

bool AudioStreamAAudio::isMMapUsed() {
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream != nullptr) {
        return AAudioExtensions::getInstance().isMMapUsed(stream);
    } else {
        return false;
    }
}

// Static method for the presentation end callback.
void AudioStreamAAudio::internalPresentationEndCallback(AAudioStream *stream, void *userData) {
    auto refs = AAudioStreamRegistry::lookupControl(userData);
    if (!refs) {
        return;
    }
    if (stream != refs.stream->mAAudioStream.load()) {
        LOGW("%s() stream already closed or closing", __func__);
        return;
    }
    spawnHelperThread(std::move(refs), [](AudioStreamAAudio *s) {
        if (auto cb = s->getPresentationCallback()) {
            cb->onPresentationEnded(s);
        }
    });
}

void AudioStreamAAudio::internalRoutingChangedCallback(
        AAudioStream *stream, void *userData, const int32_t *deviceIds, int32_t numDevices) {
    auto refs = AAudioStreamRegistry::lookupControl(userData);
    if (!refs) {
        return;
    }
    if (stream != refs.stream->mAAudioStream.load()) {
        LOGW("%s() stream already closed or closing", __func__);
        return;
    }
    std::vector<int32_t> deviceIdsCopy;
    if (deviceIds != nullptr && numDevices > 0) {
        deviceIdsCopy.assign(deviceIds, deviceIds + numDevices);
    }
    refs.stream->onRoutingChanged(deviceIdsCopy);
    if (refs.stream->getRoutingCallback() != nullptr) {
        spawnHelperThread(std::move(refs), [ids = std::move(deviceIdsCopy)](AudioStreamAAudio *s) {
            if (auto cb = s->getRoutingCallback()) {
                cb->onRoutingChanged(s, ids.data(), static_cast<int32_t>(ids.size()));
            }
        });
    }
}

Result AudioStreamAAudio::setOffloadDelayPadding(
        int32_t delayInFrames, int32_t paddingInFrames) {
    if (mLibLoader->stream_setOffloadDelayPadding == nullptr) {
        return Result::ErrorUnimplemented;
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return Result::ErrorClosed;
    }
    return static_cast<Result>(
            mLibLoader->stream_setOffloadDelayPadding(stream, delayInFrames, paddingInFrames));
}

ResultWithValue<int32_t> AudioStreamAAudio::getOffloadDelay() {
    if (mLibLoader->stream_getOffloadDelay == nullptr) {
        return ResultWithValue<int32_t>(Result::ErrorUnimplemented);
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return Result::ErrorClosed;
    }
    return ResultWithValue<int32_t>::createBasedOnSign(mLibLoader->stream_getOffloadDelay(stream));
}

ResultWithValue<int32_t> AudioStreamAAudio::getOffloadPadding() {
    if (mLibLoader->stream_getOffloadPadding == nullptr) {
        return ResultWithValue<int32_t>(Result::ErrorUnimplemented);
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
    return ResultWithValue<int32_t>::createBasedOnSign(
            mLibLoader->stream_getOffloadPadding(stream));
}

Result AudioStreamAAudio::setOffloadEndOfStream() {
    if (mLibLoader->stream_setOffloadEndOfStream == nullptr) {
        return Result::ErrorUnimplemented;
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return ResultWithValue<int32_t>(Result::ErrorClosed);
    }
    return static_cast<Result>(mLibLoader->stream_setOffloadEndOfStream(stream));
}

void AudioStreamAAudio::updateDeviceIds() {
    // If stream_getDeviceIds is not supported, use stream_getDeviceId.
    if (mLibLoader->stream_getDeviceIds == nullptr) {
        mDeviceIds.clear();
        int32_t deviceId = mLibLoader->stream_getDeviceId(mAAudioStream);
        if (deviceId != kUnspecified) {
            mDeviceIds.push_back(deviceId);
        }
    } else {
        // Allocate a temp vector with 16 elements. This should be enough to cover all cases.
        // Please file a bug on Oboe if you discover that this returns AAUDIO_ERROR_OUT_OF_RANGE.
        // When AAUDIO_ERROR_OUT_OF_RANGE is returned, the actual size will be still returned as the
        // value of deviceIdSize but deviceIds will be empty.

        static constexpr int kDefaultDeviceIdSize = 16;
        int deviceIdSize = kDefaultDeviceIdSize;
        std::vector<int32_t> deviceIds(deviceIdSize);
        aaudio_result_t getDeviceIdResult =
                mLibLoader->stream_getDeviceIds(mAAudioStream, deviceIds.data(), &deviceIdSize);
        if (getDeviceIdResult != AAUDIO_OK) {
            LOGE("stream_getDeviceIds did not return AAUDIO_OK. Error: %d",
                    static_cast<int>(getDeviceIdResult));
            return;
        }

        mDeviceIds.clear();
        for (int i = 0; i < deviceIdSize; i++) {
            mDeviceIds.push_back(deviceIds[i]);
        }
    }
    {
        const int32_t primaryId = mDeviceIds.empty() ? kUnspecified : mDeviceIds[0];
        std::lock_guard<std::mutex> lock(mUpdatedDeviceIds.lock);
        mUpdatedDeviceIds.deviceIds = mDeviceIds;
        mUpdatedDeviceIds.primaryId.store(primaryId, std::memory_order_relaxed);
    }

    // This should not happen in most cases. Please file a bug on Oboe if you see this happening.
    if (getDeviceIds().empty()) {
        LOGW("updateDeviceIds() returns an empty array.");
    }
}

ResultWithValue<int64_t> AudioStreamAAudio::flushFromFrame(FlushFromAccuracy accuracy,
                                                           int64_t positionInFrames) {
    if (mLibLoader->stream_flushFromFrame == nullptr) {
        return ResultWithValue<int64_t>(positionInFrames, Result::ErrorUnimplemented);
    }
    std::shared_lock<std::shared_mutex> lock(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        return ResultWithValue<int64_t>(positionInFrames, Result::ErrorClosed);
    }
    // TODO: use aaudio_flush_from_frame_accuracy_t when it is defined.
    auto result = static_cast<Result>(mLibLoader->stream_flushFromFrame(
                    stream, static_cast<int32_t>(accuracy), &positionInFrames));
    return ResultWithValue<int64_t>(positionInFrames, result);
}

namespace {

ResultWithValue<AAudioPlaybackParameters> oboe2AAudio_PlaybackParameters_AAudioPlaybackParameters(
        const PlaybackParameters& playbackParameters) {
    AAudioPlaybackParameters aaudioPlaybackParameters;
    switch (playbackParameters.fallbackMode) {
        case FallbackMode::Default:
            aaudioPlaybackParameters.fallbackMode = AAUDIO_FALLBACK_MODE_DEFAULT;
            break;
        case FallbackMode::Mute:
            aaudioPlaybackParameters.fallbackMode = AAUDIO_FALLBACK_MODE_MUTE;
            break;
        case FallbackMode::Fail:
            aaudioPlaybackParameters.fallbackMode = AAUDIO_FALLBACK_MODE_FAIL;
            break;
        default:
            return ResultWithValue<AAudioPlaybackParameters>(Result::ErrorIllegalArgument);
    }

    switch (playbackParameters.stretchMode) {
        case StretchMode::Default:
            aaudioPlaybackParameters.stretchMode = AAUDIO_STRETCH_MODE_DEFAULT;
            break;
        case StretchMode::Voice:
            aaudioPlaybackParameters.stretchMode = AAUDIO_STRETCH_MODE_VOICE;
            break;
        default:
            return ResultWithValue<AAudioPlaybackParameters>(Result::ErrorIllegalArgument);
    }

    aaudioPlaybackParameters.pitch = playbackParameters.pitch;
    aaudioPlaybackParameters.speed = playbackParameters.speed;
    return ResultWithValue<AAudioPlaybackParameters>(aaudioPlaybackParameters);
}

ResultWithValue<PlaybackParameters> aaudio2oboe_AAudioPlaybackParameters_PlaybackParameters(
        const AAudioPlaybackParameters& aaudioPlaybackParameters) {
    PlaybackParameters playbackParameters;
    switch (aaudioPlaybackParameters.fallbackMode) {
        case AAUDIO_FALLBACK_MODE_DEFAULT:
            playbackParameters.fallbackMode = FallbackMode::Default;
            break;
        case AAUDIO_FALLBACK_MODE_MUTE:
            playbackParameters.fallbackMode = FallbackMode::Mute;
            break;
        case AAUDIO_FALLBACK_MODE_FAIL:
            playbackParameters.fallbackMode = FallbackMode::Fail;
            break;
        default:
            LOGE("%s unknown fallback mode %d", __func__, aaudioPlaybackParameters.fallbackMode);
            return ResultWithValue<PlaybackParameters>(Result::ErrorIllegalArgument);
    }

    switch (aaudioPlaybackParameters.stretchMode) {
        case AAUDIO_STRETCH_MODE_DEFAULT:
            playbackParameters.stretchMode = StretchMode::Default;
            break;
        case AAUDIO_STRETCH_MODE_VOICE:
            playbackParameters.stretchMode = StretchMode::Voice;
            break;
        default:
            LOGE("%s unknown stretch mode %d", __func__, aaudioPlaybackParameters.stretchMode);
            return ResultWithValue<PlaybackParameters>(Result::ErrorIllegalArgument);
    }
    playbackParameters.pitch = aaudioPlaybackParameters.pitch;
    playbackParameters.speed = aaudioPlaybackParameters.speed;
    return ResultWithValue<PlaybackParameters>(playbackParameters);
}

} // namespace

Result AudioStreamAAudio::setPlaybackParameters(const PlaybackParameters &parameters) {
    if (mLibLoader->stream_setPlaybackParameters == nullptr) {
        LOGD("%s, the NDK function is not available", __func__);
        return Result::ErrorUnimplemented;
    }
    std::shared_lock _l(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        LOGE("%s the stream is already closed", __func__);
        return Result::ErrorClosed;
    }
    auto convertResult =
            oboe2AAudio_PlaybackParameters_AAudioPlaybackParameters(parameters);
    if (!convertResult) {
        LOGE("%s, invalid parameters, %s", __func__, toString(parameters).c_str());
        return Result::ErrorIllegalArgument;
    }
    auto aaudioPlaybackParameters = convertResult.value();
    return static_cast<Result>(mLibLoader->stream_setPlaybackParameters(
            stream, &aaudioPlaybackParameters));
}

ResultWithValue<PlaybackParameters> AudioStreamAAudio::getPlaybackParameters() {
    if (mLibLoader->stream_getPlaybackParameters == nullptr) {
        LOGD("%s, the NDK function is not available", __func__);
        return Result::ErrorUnimplemented;
    }
    std::shared_lock _l(mAAudioStreamLock);
    AAudioStream *stream = mAAudioStream.load();
    if (stream == nullptr) {
        LOGE("%s the stream is already closed", __func__);
        return Result::ErrorClosed;
    }

    AAudioPlaybackParameters aaudioPlaybackParameters;
    auto result = static_cast<Result>(
            mLibLoader->stream_getPlaybackParameters(stream, &aaudioPlaybackParameters));
    if (result != Result::OK) {
        return ResultWithValue<PlaybackParameters>(result);
    }

    return aaudio2oboe_AAudioPlaybackParameters_PlaybackParameters(aaudioPlaybackParameters);
}

} // namespace oboe
