#pragma once

#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack();
    ~LoopTrack();

    void prepareToPlay (const double sr, const uint maxSeconds, const uint maxBlockSize, const uint numChannels);

    void processRecord (const juce::AudioBuffer<float>& input, const int numSamples);
    void processPlayback (juce::AudioBuffer<float>& output, const int numSamples);

    void clear();
    void undo();

    const juce::AudioBuffer<float>& getAudioBuffer() const
    {
        return audioBuffer;
    }

    const juce::AudioBuffer<float>& getUndoBuffer() const
    {
        return undoBuffer;
    }

    double getSampleRate() const
    {
        return sampleRate;
    }

    int getWritePos() const
    {
        return writePos;
    }

    void setWritePos (const int newPos)
    {
        writePos = newPos;
    }

    int getLength() const
    {
        return length;
    }

    void setLength (const int newLength)
    {
        length = newLength;
    }

private:
    juce::AudioBuffer<float> audioBuffer;
    juce::AudioBuffer<float> undoBuffer;

    double sampleRate;

    int writePos = 0;
    int readPos = 0;

    int length = 0;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch);
    void updateLoopLength (const int numSamples, const int bufferSamples);
    void saveToUndoBuffer (const int ch, const int offset, const int numSamples);
    void copyInputToLoopBuffer (const int ch, const float* bufPtr, const int offset, const int numSamples);
    void advanceWritePos (const int numSamples, const int bufferSamples);
    void advanceReadPos (const int numSamples, const int bufferSamples);

    void processPlaybackChannel (juce::AudioBuffer<float>& output, const int numSamples, const int ch);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};
