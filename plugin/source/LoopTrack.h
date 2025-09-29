#pragma once

#include "LoopFifo.h"
#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack();
    ~LoopTrack();

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

    const juce::AudioBuffer<float>& getAudioBuffer() const
    {
        return audioBuffer;
    }

    const juce::AudioBuffer<float>& getUndoBuffer() const
    {
        return undoBuffer.back();
    }

    double getSampleRate() const
    {
        return sampleRate;
    }

    // int getWritePos() const
    // {
    //     return writePos;
    // }
    //
    // void setWritePos (const uint newPos)
    // {
    //     writePos = newPos;
    // }

    int getLength() const
    {
        return length;
    }

    void setLength (const int newLength)
    {
        length = newLength;
    }

    void setCrossFadeLength (const int newLength)
    {
        crossFadeLength = newLength;
    }

    bool isPrepared() const
    {
        return alreadyPrepared;
    }

    void setOverdubGains (const double oldGain, const double newGain)
    {
        overdubNewGain = std::clamp (newGain, 0.0, 2.0);
        overdubOldGain = std::clamp (oldGain, 0.0, 2.0);
    }

    double getOverdubNewGain() const
    {
        return overdubNewGain;
    }

    double getOverdubOldGain() const
    {
        return overdubOldGain;
    }

private:
    juce::AudioBuffer<float> audioBuffer;
    std::deque<juce::AudioBuffer<float>> undoBuffer;
    juce::AudioBuffer<float> tmpBuffer;

    double sampleRate;

    LoopFifo fifo;

    uint writePos = 0;
    uint readPos = 0;

    uint length = 0;
    uint provisionalLength = 0;
    uint crossFadeLength = 0;

    bool isRecording = false;
    bool alreadyPrepared = false;

    double overdubNewGain = 1.0;
    double overdubOldGain = 1.0;

    uint activeUndoLayers = 0;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const uint numSamples, const uint ch);
    void updateLoopLength (const uint numSamples, const uint bufferSamples);
    void saveToUndoBuffer();
    void copyInputToLoopBuffer (const uint ch, const float* bufPtr, const uint offset, const uint numSamples);
    void advanceWritePos (const uint numSamples, const uint bufferSamples);
    void advanceReadPos (const uint numSamples, const uint bufferSamples);

    void processPlaybackChannel (juce::AudioBuffer<float>& output, const uint numSamples, const uint ch);

    bool shouldNotRecordInputBuffer (const juce::AudioBuffer<float> input, const uint numSamples) const
    {
        return numSamples == 0 || (uint) input.getNumSamples() < numSamples || ! isPrepared()
               || input.getNumChannels() != audioBuffer.getNumChannels();
    }

    bool shouldNotPlayback (const uint numSamples, const uint ch) const
    {
        return ! isPrepared() || length == 0 || numSamples == 0 || ch >= (uint) audioBuffer.getNumChannels();
    }

    bool shouldOverdub() const
    {
        return length > 0;
    }

    static const uint MAX_SECONDS_HARD_LIMIT = 3600; // 1 hour max buffer size
    static const uint MAX_UNDO_LAYERS = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};
