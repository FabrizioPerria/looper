#pragma once

#include "LoopFifo.h"
#include "UndoBuffer.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack() {}
    ~LoopTrack() { releaseResources(); }

    void prepareToPlay (const double currentSampleRate,
                        const uint maxBlockSize,
                        const uint numChannels,
                        const uint maxSeconds = MAX_SECONDS_HARD_LIMIT,
                        const size_t maxUndoLayers = MAX_UNDO_LAYERS);
    void releaseResources();

    void processRecord (const juce::AudioBuffer<float>& input, const uint numSamples);
    void finalizeLayer();
    void processPlayback (juce::AudioBuffer<float>& output, const uint numSamples);

    void clear();
    void undo();
    void redo();

    const juce::AudioBuffer<float>& getAudioBuffer() const { return *audioBuffer; }

    size_t getCurrentReadPosition() const { return fifo.getReadPos(); }
    size_t getCurrentWritePosition() const { return fifo.getWritePos(); }

    size_t getLength() const { return length; }
    void setLength (const size_t newLength) { length = newLength; }

    double getSampleRate() const { return sampleRate; }
    int getLoopDurationSeconds() const { return (int) (length / sampleRate); }

    void setCrossFadeLength (const size_t newLength) { crossFadeLength = newLength; }

    bool isPrepared() const { return alreadyPrepared; }

    void setOverdubGains (const float oldGain, const float newGain)
    {
        PERFETTO_FUNCTION();
        overdubNewGain = std::clamp (newGain, 0.0f, 2.0f);
        overdubOldGain = std::clamp (oldGain, 0.0f, 2.0f);
        shouldNormalizeOutput = false;
    }

    void enableOutputNormalization()
    {
        PERFETTO_FUNCTION();
        overdubNewGain = 1.0f;
        overdubOldGain = 1.0f;
        shouldNormalizeOutput = true;
    }

    double getOverdubNewGain() const { return overdubNewGain; }
    double getOverdubOldGain() const { return overdubOldGain; }

    const UndoBuffer& getUndoBuffer() const { return undoBuffer; }

    void loadBackingTrack (const juce::AudioBuffer<float>& backingTrack);

    void allowWrapAround() { fifo.setWrapAround (true); }
    void preventWrapAround() { fifo.setWrapAround (false); }

    bool isCurrentlyRecording() const { return isRecording; }

    bool isMuted() const { return muted; }
    void setMuted (const bool shouldBeMuted)
    {
        PERFETTO_FUNCTION();
        static float volumeBeforeMute = 1.0f;
        if (shouldBeMuted)
        {
            if (trackVolume > 0.001f && ! muted) volumeBeforeMute = trackVolume;
            trackVolume = 0.0f;
            muted = true;
        }
        else
        {
            trackVolume = volumeBeforeMute;
            muted = false;
        }
    }

    bool isSoloed() const { return soloed; }
    void setSoloed (const bool shouldBeSoloed) { soloed = shouldBeSoloed; }

    float getTrackVolume() const { return trackVolume; }
    void setTrackVolume (const float newVolume) { trackVolume = std::clamp (newVolume, 0.0f, 1.0f); }
    void setPlaybackDirectionForward()
    {
        if (! directionForward)
        {
            directionForward = true;
            resetInterpolationFilters();
        }
    }
    void setPlaybackDirectionBackward()
    {
        if (directionForward)
        {
            directionForward = false;
            resetInterpolationFilters();
        }
    }
    bool isPlaybackDirectionForward() const { return directionForward; }

    void setPlaybackSpeed (float speed)
    {
        PERFETTO_FUNCTION();
        float newSpeed = std::clamp (speed, 0.2f, 2.0f);

        if (std::abs (playbackSpeed - newSpeed) > 0.001f)
        {
            playbackSpeed = newSpeed;
            resetInterpolationFilters();
        }
    }
    float getPlaybackSpeed() const { return playbackSpeed; }

    double getExactReadPosition() const { return fifo.getExactReadPos(); }

private:
    std::unique_ptr<juce::AudioBuffer<float>> audioBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::unique_ptr<juce::AudioBuffer<float>> tmpBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::unique_ptr<juce::AudioBuffer<float>> interpolationBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::vector<juce::LagrangeInterpolator> interpolationFilters;
    float playbackSpeed = 1.0f;

    UndoBuffer undoBuffer;

    double sampleRate = 0.0;
    int blockSize = 0;
    int channels = 0;
    int alignedBufferSize = 0;

    LoopFifo fifo;

    size_t length = 0;
    uint provisionalLength = 0;
    size_t crossFadeLength = 0;

    bool isRecording = false;
    bool alreadyPrepared = false;

    float overdubNewGain = 1.0f;
    float overdubOldGain = 1.0f;

    bool shouldNormalizeOutput = true;

    float trackVolume = 1.0f;
    float previousTrackVolume = 1.0f;

    bool muted = false;
    bool soloed = false;

    bool directionForward = true;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const uint numSamples, const uint ch);
    void updateLoopLength (const uint numSamples, const uint bufferSamples);
    void saveToUndoBuffer();

    void copyInputToLoopBuffer (const uint ch, const float* bufPtr, const uint offset, const uint numSamples);
    void copyCircularDataLinearized (int startPos, int numSamples, float speedMultiplier, int destOffset = 0);

    void advanceWritePos (const uint numSamples, const uint bufferSamples);
    void advanceReadPos (const uint numSamples, const uint bufferSamples);

    void processPlaybackChannel (juce::AudioBuffer<float>& output, const uint numSamples, const uint ch);

    void processPlaybackInterpolatedSpeed (juce::AudioBuffer<float>& output, const uint numSamples);
    void processPlaybackApplyVolume (juce::AudioBuffer<float>& output, const uint numSamples);
    void processPlaybackNormalSpeedForward (juce::AudioBuffer<float>& output, const uint numSamples);

    bool shouldNotRecordInputBuffer (const juce::AudioBuffer<float>& input, const uint numSamples) const;

    bool shouldNotPlayback (const uint numSamples) const { return ! isPrepared() || length == 0 || numSamples == 0; }

    bool shouldOverdub() const { return length > 0; }

    void resetInterpolationFilters()
    {
        for (auto& filter : interpolationFilters)
            filter.reset();
    }

    void copyAudioToTmpBuffer();

    static const uint MAX_SECONDS_HARD_LIMIT = 10 * 60; // 10 minutes
    static const uint MAX_UNDO_LAYERS = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};
