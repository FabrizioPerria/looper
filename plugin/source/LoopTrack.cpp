#include "LoopTrack.h"
#include <algorithm>
#include <cassert>

LoopTrack::LoopTrack()
{
}

LoopTrack::~LoopTrack()
{
}

void LoopTrack::prepareToPlay (const double currentSampleRate, const uint maxBlockSize, const uint numChannels)
{
    jassert (currentSampleRate > 0);
    sampleRate = currentSampleRate;
    uint totalSamples = std::max ((uint) currentSampleRate * MAX_SECONDS_HARD_LIMIT, 1u); // at least 1 block will be allocated
    uint bufferSamples = ((totalSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;
    if (bufferSamples > (uint) audioBuffer.getNumSamples())
    {
        audioBuffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }

    if (bufferSamples > (uint) undoBuffer.getNumSamples())
    {
        undoBuffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }

    clear();
}

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const int numSamples)
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

void LoopTrack::processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch)
{
    const float* inputPtr = input.getReadPointer (ch);
    int bufferSize = audioBuffer.getNumSamples();

    int currentWritePos = writePos;
    int samplesLeft = numSamples;

    while (samplesLeft > 0)
    {
        int block = std::min (samplesLeft, bufferSize - currentWritePos);
        saveToUndoBuffer (ch, currentWritePos, block);
        copyInputToLoopBuffer (ch, inputPtr, currentWritePos, block);
        samplesLeft -= block;
        inputPtr += block;
        currentWritePos = (currentWritePos + block) % bufferSize;
    }
}

void LoopTrack::saveToUndoBuffer (const int ch, const int offset, const int numSamples)
{
    jassert (numSamples > 0);
    jassert (offset >= 0);
    jassert (offset + numSamples <= undoBuffer.getNumSamples());

    auto undoPtr = undoBuffer.getWritePointer (ch);
    auto loopPtr = audioBuffer.getReadPointer (ch);

    juce::FloatVectorOperations::copy (undoPtr + offset, loopPtr + offset, numSamples);
}

void LoopTrack::copyInputToLoopBuffer (const int ch, const float* bufPtr, const int offset, const int numSamples)
{
    jassert (bufPtr != nullptr);
    jassert (numSamples > 0);
    jassert (offset >= 0);
    jassert (offset + numSamples <= audioBuffer.getNumSamples());
    auto loopPtr = audioBuffer.getWritePointer (ch);
    juce::FloatVectorOperations::add (loopPtr + offset, bufPtr, numSamples); //overdub
}

void LoopTrack::advanceWritePos (const int numSamples, const int bufferSamples)
{
    writePos = (writePos + numSamples) % bufferSamples;
}

void LoopTrack::updateLoopLength (const int numSamples, const int bufferSamples)
{
    length = std::min (length + numSamples, bufferSamples);
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, const int numSamples)
{
    jassert (output.getNumChannels() == audioBuffer.getNumChannels());
    int numChannels = audioBuffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processPlaybackChannel (output, numSamples, ch);
    }
    advanceReadPos (numSamples, length);
}

void LoopTrack::processPlaybackChannel (juce::AudioBuffer<float>& output, const int numSamples, const int ch)
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

void LoopTrack::advanceReadPos (const int numSamples, const int bufferSamples)
{
    readPos = (readPos + numSamples) % bufferSamples;
}

void LoopTrack::clear()
{
    audioBuffer.clear();
    undoBuffer.clear();
    writePos = 0;
    readPos = 0;
    length = 0;
}

void LoopTrack::undo()
{
    int numChannels = audioBuffer.getNumChannels();
    int bufferSamples = audioBuffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* loopPtr = audioBuffer.getWritePointer (ch);
        const float* undoPtr = undoBuffer.getReadPointer (ch);
        juce::FloatVectorOperations::copy (loopPtr, undoPtr, bufferSamples);
    }
}
