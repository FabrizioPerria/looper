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
    if (bufferSamples > (uint) audioBuffer.getNumSamples())
    {
        audioBuffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }
    audioBuffer.clear();

    if (bufferSamples > (uint) undoBuffer.getNumSamples())
    {
        undoBuffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }
    undoBuffer.clear();

    writePos = 0;
    readPos = 0;
    length = 0;
}

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, int numSamples)
{
    jassert (input.getNumChannels() == audioBuffer.getNumChannels());

    int numChannels = audioBuffer.getNumChannels();
    int bufferSamples = audioBuffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processRecordChannel (input, numSamples, ch);
    }

    advanceWritePos (numSamples, bufferSamples);
    updateLoopLength (numSamples, bufferSamples);
}

void LoopTrack::processRecordChannel (const juce::AudioBuffer<float>& input, int numSamples, int ch)
{
    const float* inputPtr = input.getReadPointer (ch);
    float* loopPtr = audioBuffer.getWritePointer (ch);
    float* undoPtr = undoBuffer.getWritePointer (ch);
    int bufferSize = audioBuffer.getNumSamples();

    int currentWritePos = writePos;
    int samplesLeft = numSamples;

    while (samplesLeft > 0)
    {
        int block = std::min (samplesLeft, bufferSize - currentWritePos);
        copyToUndoBuffer (loopPtr, undoPtr, currentWritePos, block);
        copyInputToLoopBuffer (inputPtr, loopPtr, currentWritePos, block);
        samplesLeft -= block;
        inputPtr += block;
        currentWritePos = (currentWritePos + block) % bufferSize;
    }
}

void LoopTrack::copyToUndoBuffer (float* bufPtr, float* undoPtr, int pos, int numSamples)
{
    jassert (bufPtr != nullptr);
    jassert (undoPtr != nullptr);

    jassert (numSamples > 0);
    jassert (pos >= 0);
    jassert (pos + numSamples <= undoBuffer.getNumSamples());
    juce::FloatVectorOperations::copy (undoPtr + pos, bufPtr + pos, numSamples);
}

void LoopTrack::copyInputToLoopBuffer (const float* inPtr, float* bufPtr, int pos, int numSamples)
{
    jassert (inPtr != nullptr);
    jassert (bufPtr != nullptr);
    jassert (numSamples > 0);
    jassert (pos >= 0);
    jassert (pos + numSamples <= audioBuffer.getNumSamples());
    juce::FloatVectorOperations::add (bufPtr + pos, inPtr, numSamples); //overdub
}

void LoopTrack::advanceWritePos (int numSamples, int bufferSamples)
{
    writePos = (writePos + numSamples) % bufferSamples;
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

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, int numSamples)
{
    jassert (output.getNumChannels() == audioBuffer.getNumChannels());
    int numChannels = audioBuffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processPlaybackChannel (output, numSamples, ch);
    }
    advanceReadPos (numSamples, length);
}

void LoopTrack::processPlaybackChannel (juce::AudioBuffer<float>& output, int numSamples, int ch)
{
    float* outPtr = output.getWritePointer (ch);
    const float* loopPtr = audioBuffer.getReadPointer (ch);

    int currentReadPos = readPos;
    int samplesLeft = numSamples;
    while (samplesLeft > 0)
    {
        int block = std::min (samplesLeft, length - currentReadPos);
        juce::FloatVectorOperations::copy (outPtr, loopPtr + currentReadPos, block);
        samplesLeft -= block;
        outPtr += block;
        currentReadPos = (currentReadPos + block) % length;
    }
}

void LoopTrack::advanceReadPos (int numSamples, int bufferSamples)
{
    readPos = (readPos + numSamples) % bufferSamples;
}

int LoopTrack::getNumSamplesToCopyBeforeEnd (int remainingSamplesToOutput, int currentPosition)
{
    int samplesUntilLoopEnds = length - currentPosition;
    return std::min (remainingSamplesToOutput, samplesUntilLoopEnds);
}
