#pragma once

#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack();
    ~LoopTrack();

    void prepareToPlay (double sr, int maxSeconds, int maxBlockSize, int numChannels);

    juce::AudioBuffer<float>& getBuffer()
    {
        return buffer;
    }

    juce::AudioBuffer<float>& getUndoBuffer()
    {
        return undoBuffer;
    }

private:
    juce::AudioBuffer<float> buffer;
    juce::AudioBuffer<float> undoBuffer;

    double sampleRate;

    int writePos = 0;
    int length = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};
