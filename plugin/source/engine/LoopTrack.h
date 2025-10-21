#pragma once

#include "LoopFifo.h"
#include "SoundTouch.h"
#include "UndoManager.h"
#include "engine/BufferManager.h"
#include "engine/VolumeProcessor.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack() {}
    ~LoopTrack() { releaseResources(); }

    void prepareToPlay (const double currentSampleRate,
                        const int maxBlockSize,
                        const int numChannels,
                        const int maxSeconds = MAX_SECONDS_HARD_LIMIT,
                        const int maxUndoLayers = MAX_UNDO_LAYERS);
    void releaseResources();

    void processRecord (const juce::AudioBuffer<float>& input, const int numSamples);
    void finalizeLayer();
    void processPlayback (juce::AudioBuffer<float>& output, const int numSamples);

    void clear();
    void undo();
    void redo();

    int getCurrentReadPosition() const { return bufferManager.getFifo().getReadPos(); }
    int getCurrentWritePosition() const { return bufferManager.getFifo().getWritePos(); }
    double getSampleRate() const { return sampleRate; }
    int getLoopDurationSeconds() const { return (int) (bufferManager.getLength() / sampleRate); }

    void setCrossFadeLength (const int newCrossFadeLength) { volumeProcessor.setCrossFadeLength (newCrossFadeLength); }

    void loadBackingTrack (const juce::AudioBuffer<float>& backingTrack);
    juce::AudioBuffer<float>* getAudioBuffer() { return bufferManager.getAudioBuffer().get(); }

    const int getAvailableTrackSizeSamples() const { return (int) alignedBufferSize; }

    bool isCurrentlyRecording() const { return isRecording; }

    bool isPlaybackDirectionForward() const { return playheadDirection > 0; }
    void setPlaybackDirectionForward()
    {
        if (! isPlaybackDirectionForward())
        {
            playheadDirection = 1;
            bufferManager.getFifo().setPlaybackRate (playbackSpeed, playheadDirection);
        }
    }
    void setPlaybackDirectionBackward()
    {
        if (isPlaybackDirectionForward())
        {
            playheadDirection = -1;
            bufferManager.getFifo().setPlaybackRate (playbackSpeed, playheadDirection);
        }
    }

    void setPlaybackSpeed (float speed)
    {
        PERFETTO_FUNCTION();
        float newSpeed = std::clamp (speed, 0.2f, 2.0f);

        if (std::abs (playbackSpeed - newSpeed) > 0.001f)
        {
            playbackSpeed = newSpeed;
            bufferManager.getFifo().setPlaybackRate (playbackSpeed, playheadDirection);
        }
    }
    float getPlaybackSpeed() const { return playbackSpeed; }

    double getExactReadPosition() const { return bufferManager.getFifo().getExactReadPos(); }

    bool shouldKeepPitchWhenChangingSpeed() const { return keepPitchWhenChangingSpeed; }
    void setKeepPitchWhenChangingSpeed (const bool shouldKeepPitch)
    {
        for (auto& st : soundTouchProcessors)
        {
            st->flush();
            st->clear();
        }

        keepPitchWhenChangingSpeed = shouldKeepPitch;
    }

    bool hasWrappedAround() { return bufferManager.hasWrappedAround(); }

    float getTrackVolume() const { return volumeProcessor.getTrackVolume(); }
    void setTrackVolume (const float newVolume) { volumeProcessor.setTrackVolume (newVolume); }

    void setOverdubGains (const double oldGain, const double newGain) { volumeProcessor.setOverdubGains (oldGain, newGain); }

    bool isSoloed() const { return volumeProcessor.isSoloed(); }
    void setSoloed (const bool shouldBeSoloed) { volumeProcessor.setSoloed (shouldBeSoloed); }

    bool isMuted() const { return volumeProcessor.isMuted(); }
    void setMuted (const bool shouldBeMuted) { volumeProcessor.setMuted (shouldBeMuted); }

    int getTrackLengthSamples() const { return bufferManager.getLength(); }

private:
    VolumeProcessor volumeProcessor;
    BufferManager bufferManager;
    UndoStackManager undoManager;

    std::unique_ptr<juce::AudioBuffer<float>> interpolationBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::vector<std::unique_ptr<soundtouch::SoundTouch>> soundTouchProcessors;
    std::vector<float> zeroBuffer;

    bool keepPitchWhenChangingSpeed = false;

    float previousSpeedMultiplier = 1.0f;

    float previousPlaybackSpeed = 1.0f;
    float playbackSpeed = 1.0f;

    bool previousKeepPitch = false;
    bool wasUsingFastPath = true;

    int playheadDirection = 1; // 1 = forward, -1 = backward
    float playbackSpeedBeforeRecording = 1.0f;
    int playheadDirectionBeforeRecording = 1;

    double sampleRate = 0.0;
    int blockSize = 0;
    int channels = 0;
    size_t alignedBufferSize = 0;

    bool isRecording = false;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch);

    void copyCircularDataLinearized (int startPos, int numSamples, float speedMultiplier, int destOffset = 0);

    void advanceWritePos (const int numSamples, const int bufferSamples);
    void advanceReadPos (const int numSamples, const int bufferSamples);

    void processPlaybackChannel (juce::AudioBuffer<float>& output, const int numSamples, const int ch);

    void processPlaybackInterpolatedSpeedWithPitchCorrection (juce::AudioBuffer<float>& output, const int numSamples);
    void processPlaybackInterpolatedSpeed (juce::AudioBuffer<float>& output, const int numSamples);
    void processPlaybackNormalSpeedForward (juce::AudioBuffer<float>& output, const int numSamples);

    bool shouldNotRecordInputBuffer (const juce::AudioBuffer<float>& input, const int numSamples) const;

    bool shouldNotPlayback (const int numSamples) const { return bufferManager.getLength() == 0 || numSamples <= 0; }

    static const int MAX_SECONDS_HARD_LIMIT = 10 * 60; // 10 minutes
    static const int MAX_UNDO_LAYERS = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};
