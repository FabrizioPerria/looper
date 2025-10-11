#pragma once

#include "LoopFifo.h"
#include "PerfettoProfiler.h"
#include "UndoBuffer.h"
#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack()
    {
    }

    ~LoopTrack()
    {
        releaseResources();
    }

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

    const juce::AudioBuffer<float>& getAudioBuffer() const
    {
        PERFETTO_FUNCTION();
        return *audioBuffer;
    }

    double getSampleRate() const
    {
        PERFETTO_FUNCTION();
        return sampleRate;
    }

    size_t getLength() const
    {
        PERFETTO_FUNCTION();
        return length;
    }

    size_t getCurrentReadPosition() const
    {
        PERFETTO_FUNCTION();
        return fifo.getReadPos();
    }

    int getLoopDurationSeconds() const
    {
        PERFETTO_FUNCTION();
        return (int) (length / sampleRate);
    }

    void setLength (const size_t newLength)
    {
        PERFETTO_FUNCTION();
        length = newLength;
    }

    void setCrossFadeLength (const size_t newLength)
    {
        PERFETTO_FUNCTION();
        crossFadeLength = newLength;
    }

    bool isPrepared() const
    {
        PERFETTO_FUNCTION();
        return alreadyPrepared;
    }

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

    double getOverdubNewGain() const
    {
        PERFETTO_FUNCTION();
        return overdubNewGain;
    }

    double getOverdubOldGain() const
    {
        PERFETTO_FUNCTION();
        return overdubOldGain;
    }

    const UndoBuffer& getUndoBuffer() const
    {
        PERFETTO_FUNCTION();
        return undoBuffer;
    }

    void loadBackingTrack (const juce::AudioBuffer<float>& backingTrack);

    void allowWrapAround()
    {
        PERFETTO_FUNCTION();
        fifo.setWrapAround (true);
    }
    void preventWrapAround()
    {
        PERFETTO_FUNCTION();
        fifo.setWrapAround (false);
    }

    bool isCurrentlyRecording() const
    {
        PERFETTO_FUNCTION();
        return isRecording;
    }

    bool isMuted() const
    {
        PERFETTO_FUNCTION();
        return muted;
    }

    void setMuted (const bool shouldBeMuted)
    {
        PERFETTO_FUNCTION();
        muted = shouldBeMuted;
    }

    float getTrackVolume() const
    {
        PERFETTO_FUNCTION();
        return trackVolume;
    }

    void setTrackVolume (const float newVolume)
    {
        PERFETTO_FUNCTION();
        trackVolume = std::clamp (newVolume, 0.0f, 1.0f);
    }

private:
    std::unique_ptr<juce::AudioBuffer<float>> audioBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::unique_ptr<juce::AudioBuffer<float>> tmpBuffer = std::make_unique<juce::AudioBuffer<float>>();

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

    bool muted = false;

    float trackVolume = 1.0f;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const uint numSamples, const uint ch);
    void updateLoopLength (const uint numSamples, const uint bufferSamples);
    void saveToUndoBuffer();
    void copyInputToLoopBuffer (const uint ch, const float* bufPtr, const uint offset, const uint numSamples);
    void advanceWritePos (const uint numSamples, const uint bufferSamples);
    void advanceReadPos (const uint numSamples, const uint bufferSamples);

    void processPlaybackChannel (juce::AudioBuffer<float>& output, const uint numSamples, const uint ch);

    bool shouldNotRecordInputBuffer (const juce::AudioBuffer<float>& input, const uint numSamples) const
    {
        PERFETTO_FUNCTION();
        return numSamples == 0 || (uint) input.getNumSamples() < numSamples || ! isPrepared()
               || input.getNumChannels() != audioBuffer->getNumChannels();
    }

    bool shouldNotPlayback (const uint numSamples) const
    {
        PERFETTO_FUNCTION();
        return ! isPrepared() || length == 0 || numSamples == 0;
    }

    bool shouldOverdub() const
    {
        PERFETTO_FUNCTION();
        return length > 0;
    }

    void copyAudioToTmpBuffer()
    {
        PERFETTO_FUNCTION();
        // Copy current audioBuffer state to tmpBuffer
        // This prepares tmpBuffer to be pushed to undo stack on next overdub
        for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
        {
            juce::FloatVectorOperations::copy (tmpBuffer->getWritePointer (ch), audioBuffer->getReadPointer (ch), (int) length);
        }
    }

    static const uint MAX_SECONDS_HARD_LIMIT = 300; // 5mins
    static const uint MAX_UNDO_LAYERS = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};
