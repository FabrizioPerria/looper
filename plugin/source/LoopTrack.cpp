#include "LoopTrack.h"
#include <algorithm>
#include <cassert>

LoopTrack::LoopTrack()
{
}

LoopTrack::~LoopTrack()
{
}

void LoopTrack::prepareToPlay (double sr, uint maxSeconds, uint maxBlockSize, uint numChannels)
{
    jassert (sr > 0);
    sampleRate = sr;
    uint totalSamples = std::max ((uint) sr * maxSeconds, 1u); // at least 1 block will be allocated
    uint bufferSamples = ((totalSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;
    if (bufferSamples > audioBuffer.getNumSamples())
    {
        audioBuffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }
    audioBuffer.clear();

    if (bufferSamples > undoBuffer.getNumSamples())
    {
        undoBuffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }
    undoBuffer.clear();

    writePos = 0;
    length = 0;
}

void LoopTrack::processBlock (const juce::AudioBuffer<float>& input, int numSamples)
{
    jassert (input.getNumChannels() == audioBuffer.getNumChannels());

    int numChannels = audioBuffer.getNumChannels();
    int bufferSamples = audioBuffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processChannel (input, numSamples, ch);
    }

    updateLoopLength (numSamples, bufferSamples);
}

void LoopTrack::processChannel (juce::AudioBuffer<float> input, int numSamples, int ch)
{
    const float* inPtr = input.getReadPointer (ch);
    float* bufPtr = audioBuffer.getWritePointer (ch);
    float* undoPtr = undoBuffer.getWritePointer (ch);
    int bufferSamples = audioBuffer.getNumSamples();

    int samplesToEnd = bufferSamples - writePos;
    int samplesThisLoop = std::min (numSamples, samplesToEnd);
    int samplesRemaining = numSamples;

    copyToUndoBuffer (bufPtr, undoPtr, writePos, samplesThisLoop);
    copyInputToLoopBuffer (inPtr, bufPtr, writePos, samplesThisLoop);

    advanceWritePos (samplesThisLoop, bufferSamples);
    samplesRemaining -= samplesThisLoop;

    if (samplesRemaining > 0)
    {
        // wrap around
        copyToUndoBuffer (bufPtr, undoPtr, writePos, samplesRemaining);
        copyInputToLoopBuffer (inPtr + samplesThisLoop, bufPtr, writePos, samplesRemaining);

        advanceWritePos (samplesRemaining, bufferSamples);
    }
}

void LoopTrack::copyToUndoBuffer (float* bufPtr, float* undoPtr, int pos, int numSamples)
{
    std::copy (bufPtr + pos, bufPtr + pos + numSamples, undoPtr + pos);
}

void LoopTrack::copyInputToLoopBuffer (const float* inPtr, float* bufPtr, int pos, int numSamples)
{
    // std::copy (inPtr, inPtr + numSamples, bufPtr + pos);
    // but we want to add to existing buffer content (overdubbing)
    for (int i = 0; i < numSamples; ++i)
        bufPtr[pos + i] += inPtr[i];
}

void LoopTrack::advanceWritePos (int numSamples, int bufferSamples)
{
    writePos += numSamples;
    if (writePos >= bufferSamples)
    {
        writePos = 0;
    }
}

void LoopTrack::updateLoopLength (int numSamples, int bufferSamples)
{
    if (length < bufferSamples)
    {
        length += numSamples;
        if (length > bufferSamples)
        {
            length = bufferSamples;
        }
    }
}
