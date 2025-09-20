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
    DBG ("Processing record channel " << ch);
    const float* inputPtr = input.getReadPointer (ch);
    float* loopPtr = audioBuffer.getWritePointer (ch);
    float* undoPtr = undoBuffer.getWritePointer (ch);
    int bufferSize = audioBuffer.getNumSamples();
    DBG ("  Buffer size: " << bufferSize << " Write Pos: " << writePos);

    int currentWritePos = writePos;
    int samplesLeft = numSamples;
    DBG ("  Write Pos Start: " << writePos << " Samples to write: " << numSamples);

    while (samplesLeft > 0)
    {
        int block = std::min (samplesLeft, bufferSize - currentWritePos);
        DBG ("    Copying block of " << block << " from pos " << currentWritePos);
        copyToUndoBuffer (loopPtr, undoPtr, currentWritePos, block);
        DBG ("    Undo buffer pos after copy: " << (currentWritePos + block) % bufferSize);
        copyInputToLoopBuffer (inputPtr, loopPtr, currentWritePos, block);
        DBG ("    Loop buffer pos after copy: " << (currentWritePos + block) % bufferSize);
        samplesLeft -= block;
        DBG ("    Samples Left: " << samplesLeft);
        inputPtr += block;
        DBG ("    Input Ptr advanced to: " << (inputPtr - input.getReadPointer (ch)));
        currentWritePos = (currentWritePos + block) % bufferSize;
        DBG ("    Current Write Pos updated to: " << currentWritePos);
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
    DBG ("    Copied " << numSamples << " samples to undo buffer at pos " << pos);
}

void LoopTrack::copyInputToLoopBuffer (const float* inPtr, float* bufPtr, int pos, int numSamples)
{
    jassert (inPtr != nullptr);
    jassert (bufPtr != nullptr);
    jassert (numSamples > 0);
    jassert (pos >= 0);
    jassert (pos + numSamples <= audioBuffer.getNumSamples());
    juce::FloatVectorOperations::add (bufPtr + pos, inPtr, numSamples); //overdub
    DBG ("    Copied " << numSamples << " samples to loop buffer at pos " << pos);
}

void LoopTrack::advanceWritePos (int numSamples, int bufferSamples)
{
    writePos = (writePos + numSamples) % bufferSamples;
    DBG ("Write Pos End: " << writePos);
}

void LoopTrack::updateLoopLength (int numSamples, int bufferSamples)
{
    length = std::min (length + numSamples, bufferSamples);
    DBG ("Updated loop length: " << length);
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
    DBG ("Processing playback channel " << ch);
    float* outPtr = output.getWritePointer (ch);
    const float* loopPtr = audioBuffer.getReadPointer (ch);
    DBG ("  Buffer size: " << audioBuffer.getNumSamples() << " Length: " << length);

    int currentReadPos = readPos;
    int samplesLeft = numSamples;
    DBG ("  Read Pos Start: " << readPos << " Length: " << length);
    while (samplesLeft > 0)
    {
        int block = std::min (samplesLeft, length - currentReadPos);
        DBG ("    Copying block of " << block << " from pos " << currentReadPos);
        juce::FloatVectorOperations::copy (outPtr, loopPtr + currentReadPos, block);
        DBG ("    Read Pos After Copy: " << (currentReadPos + block) % length);
        samplesLeft -= block;
        DBG ("    Samples Left: " << samplesLeft);
        outPtr += block;
        DBG ("    Out Ptr advanced to: " << (outPtr - output.getWritePointer (ch)));
        currentReadPos = (currentReadPos + block) % length;
        DBG ("    Current Read Pos updated to: " << currentReadPos);
    }
}

void LoopTrack::advanceReadPos (int numSamples, int bufferSamples)
{
    readPos = (readPos + numSamples) % bufferSamples;
    DBG ("Read Pos End: " << readPos << " Length: " << length);
}
