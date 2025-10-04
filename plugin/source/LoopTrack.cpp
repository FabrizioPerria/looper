#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <algorithm>
#include <cassert>

//==============================================================================
// Setup
//==============================================================================

void LoopTrack::prepareToPlay (const double currentSampleRate,
                               const uint maxBlockSize,
                               const uint numChannels,
                               const uint maxSeconds,
                               const size_t maxUndoLayers)
{
    if (isPrepared() || currentSampleRate <= 0.0 || ! maxBlockSize || ! numChannels || ! maxSeconds) return;

    sampleRate = currentSampleRate;
    const uint requestedSamples = std::max ((uint) currentSampleRate * maxSeconds, 1u); // at least 1 block will be allocated
    const uint alignedBufferSize = ((requestedSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;

    if (alignedBufferSize > (uint) audioBuffer->getNumSamples())
    {
        audioBuffer->setSize ((int) numChannels, (int) alignedBufferSize, false, true, true);
        tmpBuffer->setSize ((int) numChannels, (int) alignedBufferSize, false, true, true);
        undoStaging->setSize ((int) numChannels, (int) alignedBufferSize, false, true, true);
    }

    fifo.prepareToPlay ((int) alignedBufferSize);
    undoBuffer.prepareToPlay ((int) maxUndoLayers, (int) numChannels, (int) alignedBufferSize);

    clear();
    setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade

    alreadyPrepared = true;
}

void LoopTrack::releaseResources()
{
    if (! isPrepared()) return;

    clear();
    audioBuffer->setSize (0, 0, false, false, true);
    tmpBuffer->setSize (0, 0, false, false, true);

    undoBuffer.releaseResources();
    fifo.clear();

    sampleRate = 0.0;
    alreadyPrepared = false;
}

//==============================================================================
// Recording
//==============================================================================

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const uint numSamples)
{
    if (shouldNotRecordInputBuffer (input, numSamples)) return;

    if (! isRecording)
    {
        isRecording = true;
        saveToUndoBuffer();
    }

    int writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap;
    fifo.prepareToWrite ((int) numSamples, writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap);

    const int actualWritten = samplesBeforeWrap + samplesAfterWrap;

    for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
    {
        if (samplesBeforeWrap > 0)
            copyInputToLoopBuffer ((uint) ch, input.getReadPointer (ch), (uint) writePosBeforeWrap, (uint) samplesBeforeWrap);
        if (samplesAfterWrap > 0 && shouldOverdub())
            copyInputToLoopBuffer ((uint) ch,
                                   input.getReadPointer (ch) + samplesBeforeWrap,
                                   (uint) writePosAfterWrap,
                                   (uint) samplesAfterWrap);
    }

    fifo.finishedWrite (actualWritten, shouldOverdub());

    updateLoopLength ((uint) actualWritten, (uint) audioBuffer->getNumSamples());
}

void LoopTrack::saveToUndoBuffer()
{
    if (! isPrepared() || ! shouldOverdub()) return;

    for (int ch = 0; ch < tmpBuffer->getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::copy (undoStaging->getWritePointer (ch), tmpBuffer->getReadPointer (ch), (int) length);
    }

    // Push staging buffer to undo (swaps undoStaging, not tmpBuffer)
    undoBuffer.pushLayer (undoStaging, length);

    // Note: tmpBuffer is now unchanged and remains valid for overdubbing reference
}

void LoopTrack::copyInputToLoopBuffer (const uint ch, const float* bufPtr, const uint offset, const uint numSamples)
{
    if (! isRecording) return;

    auto* dest = audioBuffer->getWritePointer ((int) ch) + offset;

    juce::FloatVectorOperations::multiply (dest, shouldOverdub() ? overdubOldGain : 0, (int) numSamples);
    juce::FloatVectorOperations::addWithMultiply (dest, bufPtr, overdubNewGain, (int) numSamples);
}

void LoopTrack::updateLoopLength (const uint numSamples, const uint bufferSamples)
{
    provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
}

void LoopTrack::finalizeLayer()
{
    const size_t currentLength = std::max ({ (int) length, (int) provisionalLength, 1 });
    if (length == 0)
    {
        fifo.setMusicalLength ((int) currentLength);
        length = currentLength;
    }
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

    const uint fadeSamples = std::min (crossFadeLength, length / 4);
    if (fadeSamples > 0)
    {
        audioBuffer->applyGainRamp (0, fadeSamples, 0.0f, overallGain);                    // fade in
        audioBuffer->applyGainRamp (length - fadeSamples, fadeSamples, overallGain, 0.0f); // fade out
    }

    copyAudioToTmpBuffer();
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, const uint numSamples)
{
    if (shouldNotPlayback (numSamples)) return;

    int readPosBeforeWrap, samplesBeforeWrap, readPosAfterWrap, samplesAfterWrap;
    fifo.prepareToRead ((int) numSamples, readPosBeforeWrap, samplesBeforeWrap, readPosAfterWrap, samplesAfterWrap);
    const int actualRead = samplesBeforeWrap + samplesAfterWrap;

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        float* outPtr = output.getWritePointer (ch);
        const float* loopPtr = audioBuffer->getReadPointer (ch);

        if (samplesBeforeWrap > 0) juce::FloatVectorOperations::add (outPtr, loopPtr + readPosBeforeWrap, samplesBeforeWrap);
        if (samplesAfterWrap > 0)
            juce::FloatVectorOperations::add (outPtr + samplesBeforeWrap, loopPtr + readPosAfterWrap, samplesAfterWrap);
    }

    fifo.finishedRead (actualRead, shouldOverdub());
}

void LoopTrack::clear()
{
    audioBuffer->clear();
    undoBuffer.clear();
    // tmpBuffer.clear();
    length = 0;
    provisionalLength = 0;
}

void LoopTrack::undo()
{
    if (! shouldOverdub() || ! isPrepared()) return;

    if (undoBuffer.undo (audioBuffer))
    {
        finalizeLayer();
    }
}

void LoopTrack::redo()
{
    if (! shouldOverdub() || ! isPrepared()) return;

    if (undoBuffer.redo (audioBuffer))
    {
        finalizeLayer();
    }
}
