#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <algorithm>
#include <cassert>

LoopTrack::LoopTrack() {}

LoopTrack::~LoopTrack() {}

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

    tmpBuffer.setSize (1, maxBlockSize, false, true, true);

    clear();

    setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade
}

void LoopTrack::releaseResources()
{
    clear();
    audioBuffer.setSize (0, 0, false, false, true);
    undoBuffer.setSize (0, 0, false, false, true);
    tmpBuffer.setSize (0, 0, false, false, true);
    sampleRate = 0.0;
}

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const int numSamples)
{
    jassert (input.getNumChannels() == audioBuffer.getNumChannels());

    if (! isRecording)
    {
        isRecording = true;
        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
        {
            saveToUndoBuffer (ch, 0, length);
        }
    }

    const int numChannels = audioBuffer.getNumChannels();
    const int bufferSamples = audioBuffer.getNumSamples();

    int samplesCanRecord = std::min (numSamples, bufferSamples - provisionalLength);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processRecordChannel (input, samplesCanRecord, ch);
    }

    if (! isOverdubbing())
    {
        advanceWritePos (samplesCanRecord, bufferSamples);
        updateLoopLength (samplesCanRecord, bufferSamples);
    }

    if (samplesCanRecord < numSamples) finalizeLayer();
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
        if (block <= 0) break;
        copyInputToLoopBuffer (ch, inputPtr, currentWritePos, block);
        samplesLeft -= block;
        inputPtr += block;
        currentWritePos = (currentWritePos + block) % bufferSize;
    }
}

void LoopTrack::saveToUndoBuffer (const int ch, const int offset, const int numSamples)
{
    auto undoPtr = undoBuffer.getWritePointer (ch);
    const auto loopPtr = audioBuffer.getReadPointer (ch);

    juce::FloatVectorOperations::copy (undoPtr + offset, loopPtr + offset, numSamples);
}

void LoopTrack::copyInputToLoopBuffer (const int ch, const float* bufPtr, const int offset, const int numSamples)
{
    auto* loopPtr = audioBuffer.getWritePointer (ch);

    if (isRecording && isOverdubbing())
    {
        const float overdubGain = 1.0f;    // scale new layer
        const float overdubOldGain = 1.0f; // scale old layer

        juce::FloatVectorOperations::copyWithMultiply (loopPtr + offset, loopPtr + offset, overdubOldGain, numSamples);

        auto* tmpPtr = tmpBuffer.getWritePointer (0);
        juce::FloatVectorOperations::copyWithMultiply (tmpPtr, bufPtr, overdubGain, numSamples);
        juce::FloatVectorOperations::add (loopPtr + offset, tmpPtr, numSamples);
    }
    else if (isRecording)
    {
        juce::FloatVectorOperations::copy (loopPtr + offset, bufPtr, numSamples);
    }
}

void LoopTrack::advanceWritePos (const int numSamples, const int bufferSamples) { writePos = (writePos + numSamples) % bufferSamples; }

void LoopTrack::updateLoopLength (const int numSamples, const int bufferSamples)
{
    provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
}

void LoopTrack::finalizeLayer()
{
    length = std::max (provisionalLength, length);
    provisionalLength = 0;
    isRecording = false;

    // float maxSample = 0.0f;
    // for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    //     maxSample = std::max (maxSample, audioBuffer.getMagnitude (ch, 0, length));
    //
    // if (maxSample > 0.0f)
    //     audioBuffer.applyGain (0, length, 1.0f / maxSample);

    const float overallGain = 1.0f;
    // audioBuffer.applyGain (overallGain);

    const int fadeSamples = std::min (crossFadeLength, length / 4);
    if (fadeSamples <= 0) return;

    audioBuffer.applyGainRamp (0, fadeSamples, 0.0f, overallGain);                    // fade in
    audioBuffer.applyGainRamp (length - fadeSamples, fadeSamples, overallGain, 0.0f); // fade out
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
        if (block <= 0) break;
        juce::FloatVectorOperations::add (outPtr, loopPtr + currentReadPos, block);
        samplesLeft -= block;
        outPtr += block;
        currentReadPos = (currentReadPos + block) % length;
    }
}

void LoopTrack::advanceReadPos (const int numSamples, const int bufferSamples)
{
    readPos = (readPos + numSamples) % bufferSamples;
    if (isOverdubbing()) writePos = readPos; // keep write position in sync for overdubbing
}

void LoopTrack::clear()
{
    audioBuffer.clear();
    undoBuffer.clear();
    tmpBuffer.clear();
    writePos = readPos = length = provisionalLength = 0;
}

void LoopTrack::undo()
{
    int numChannels = audioBuffer.getNumChannels();
    int bufferSamples = audioBuffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* loopPtr = audioBuffer.getWritePointer (ch);
        const float* undoPtr = undoBuffer.getReadPointer (ch);
        juce::FloatVectorOperations::copy (loopPtr, undoPtr, length);
    }
    finalizeLayer();
}
