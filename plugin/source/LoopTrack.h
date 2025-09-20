#pragma once

#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack();
    ~LoopTrack();

    void prepareToPlay (double sr, uint maxSeconds, uint maxBlockSize, uint numChannels);

    juce::AudioBuffer<float>& getBuffer()
    {
        return buffer;
    }

    juce::AudioBuffer<float>& getUndoBuffer()
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

    void setWritePos (int newPos)
    {
        writePos = newPos;
    }

    int getLength() const
    {
        return length;
    }

    void setLength (int newLength)
    {
        length = newLength;
    }

private:
    juce::AudioBuffer<float> buffer;
    juce::AudioBuffer<float> undoBuffer;

    double sampleRate;

    int writePos = 0;
    int length = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};
