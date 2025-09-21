#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
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
    prepareToPlay (currentSampleRate, MAX_SECONDS_HARD_LIMIT, maxBlockSize, numChannels);
}

void LoopTrack::prepareToPlay (const double currentSampleRate, const uint maxSeconds, const uint maxBlockSize, const uint numChannels)
{
    jassert (currentSampleRate > 0);
    sampleRate = currentSampleRate;
    uint totalSamples = std::max ((uint) currentSampleRate * maxSeconds, 1u); // at least 1 block will be allocated
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

void LoopTrack::releaseResources()
{
    clear();
    audioBuffer.setSize (0, 0, false, false, true);
    undoBuffer.setSize (0, 0, false, false, true);
    sampleRate = 0.0;
}

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const int numSamples)
{
    jassert (input.getNumChannels() == audioBuffer.getNumChannels());

    isRecording = true;

    const int numChannels = audioBuffer.getNumChannels();
    const int bufferSamples = audioBuffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processRecordChannel (input, numSamples, ch);
    }

    if (! isOverdubbing())
    {
        advanceWritePos (numSamples, bufferSamples);
        updateLoopLength (numSamples, bufferSamples);
    }
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
    auto undoPtr = undoBuffer.getWritePointer (ch);
    auto loopPtr = audioBuffer.getReadPointer (ch);

    juce::FloatVectorOperations::copy (undoPtr + offset, loopPtr + offset, numSamples);
}

void LoopTrack::copyInputToLoopBuffer (const int ch, const float* bufPtr, const int offset, const int numSamples)
{
    auto loopPtr = audioBuffer.getWritePointer (ch);
    if (isRecording)
    {
        juce::FloatVectorOperations::add (loopPtr + offset, bufPtr, numSamples);
    }
}

void LoopTrack::advanceWritePos (const int numSamples, const int bufferSamples)
{
    writePos = (writePos + numSamples) % bufferSamples;
}

void LoopTrack::updateLoopLength (const int numSamples, const int bufferSamples)
{
    provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
}

void LoopTrack::finalizeLayer()
{
    length = std::max (provisionalLength, length);
    provisionalLength = 0;
    isRecording = false;
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
        if (block <= 0)
        {
            break;
        }
        juce::FloatVectorOperations::add (outPtr, loopPtr + currentReadPos, block);
        samplesLeft -= block;
        outPtr += block;
        currentReadPos = (currentReadPos + block) % length;
    }
}

void LoopTrack::advanceReadPos (const int numSamples, const int bufferSamples)
{
    readPos = (readPos + numSamples) % bufferSamples;
    if (isOverdubbing())
    {
        writePos = readPos; // keep write position in sync for overdubbing
    }
}

void LoopTrack::clear()
{
    audioBuffer.clear();
    undoBuffer.clear();
    writePos = 0;
    readPos = 0;
    length = 0;
    provisionalLength = 0;
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
