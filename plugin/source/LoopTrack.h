#pragma once

#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack();
    ~LoopTrack();

    void prepareToPlay (double sr, uint maxSeconds, uint maxBlockSize, uint numChannels);

    void processRecord (const juce::AudioBuffer<float>& input, int numSamples);
    void processPlayback (juce::AudioBuffer<float>& output, int numSamples);

    juce::AudioBuffer<float>& getAudioBuffer()
    {
        return audioBuffer;
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
    juce::AudioBuffer<float> audioBuffer;
    juce::AudioBuffer<float> undoBuffer;

    double sampleRate;

    int writePos = 0;
    int readPos = 0;

    int length = 0;

    int processRecordChannel (const juce::AudioBuffer<float>& input, int numSamples, int ch);
    void updateLoopLength (int numSamples, int bufferSamples);
    void copyToUndoBuffer (float* bufPtr, float* undoPtr, int pos, int numSamples);
    void copyInputToLoopBuffer (const float* inPtr, float* bufPtr, int pos, int numSamples);
    void advanceWritePos (int numSamples, int bufferSamples);
    void advanceReadPos (int numSamples, int bufferSamples);

    void processPlaybackChannel (juce::AudioBuffer<float>& output, int numSamples, int ch);
    int getNumSamplesToCopyBeforeEnd (int remainingSamplesToOutput, int currentPosition);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};
