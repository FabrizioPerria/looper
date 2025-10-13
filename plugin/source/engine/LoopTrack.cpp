#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "profiler/PerfettoProfiler.h"
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
    PERFETTO_FUNCTION();
    if (isPrepared() || currentSampleRate <= 0.0 || ! maxBlockSize || ! numChannels || ! maxSeconds) return;

    sampleRate = currentSampleRate;
    blockSize = (int) maxBlockSize;
    channels = (int) numChannels;
    const uint requestedSamples = std::max ((uint) currentSampleRate * maxSeconds, 1u); // at least 1 block will be allocated
    alignedBufferSize = ((requestedSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;

    if (alignedBufferSize > (uint) audioBuffer->getNumSamples())
    {
        audioBuffer->setSize ((int) numChannels, (int) alignedBufferSize, false, true, true);
        tmpBuffer->setSize ((int) numChannels, (int) alignedBufferSize, false, true, true);
    }

    undoBuffer.prepareToPlay ((int) maxUndoLayers, (int) numChannels, (int) alignedBufferSize);

    clear();

    setCrossFadeLength ((int) (0.03 * sampleRate)); // default 30 ms crossfade

    alreadyPrepared = true;
}

void LoopTrack::releaseResources()
{
    PERFETTO_FUNCTION();
    if (! isPrepared()) return;

    undoBuffer.releaseResources();
    clear();
    audioBuffer->setSize (0, 0, false, false, true);
    tmpBuffer->setSize (0, 0, false, false, true);

    fifo.clear();

    sampleRate = 0.0;
    alreadyPrepared = false;
}

//==============================================================================
// Recording
//==============================================================================

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const uint numSamples)
{
    PERFETTO_FUNCTION();
    if (shouldNotRecordInputBuffer (input, numSamples)) return;

    if (! isRecording)
    {
        isRecording = true;
        saveToUndoBuffer();
    }

    int writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap;
    fifo.prepareToWrite ((int) numSamples, writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap);

    for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
    {
        if (samplesBeforeWrap > 0)
        {
            copyInputToLoopBuffer ((uint) ch, input.getReadPointer (ch), (uint) writePosBeforeWrap, (uint) samplesBeforeWrap);
        }
        if (samplesAfterWrap > 0 && shouldOverdub())
        {
            copyInputToLoopBuffer ((uint) ch,
                                   input.getReadPointer (ch) + samplesBeforeWrap,
                                   (uint) writePosAfterWrap,
                                   (uint) samplesAfterWrap);
        }
    }

    int actualWritten = samplesBeforeWrap + samplesAfterWrap;
    fifo.finishedWrite (actualWritten, shouldOverdub());

    bool fifoPreventedWrap = ! fifo.getWrapAround() && samplesAfterWrap == 0 && numSamples > samplesBeforeWrap;
    if (fifoPreventedWrap)
    {
        finalizeLayer();
        return;
    }

    updateLoopLength ((uint) samplesBeforeWrap, shouldOverdub() ? length : (uint) audioBuffer->getNumSamples());
}

void LoopTrack::saveToUndoBuffer()
{
    PERFETTO_FUNCTION();
    if (! isPrepared() || ! shouldOverdub()) return;

    undoBuffer.finalizeCopyAndPush (tmpBuffer, length);
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
    PERFETTO_FUNCTION();
    const size_t currentLength = std::max ({ (int) length, (int) provisionalLength, 1 });
    if (length == 0)
    {
        fifo.setMusicalLength ((int) currentLength);
        length = currentLength;
    }
    provisionalLength = 0;
    isRecording = false;

    if (shouldNormalizeOutput)
    {
        float maxSample = 0.0f;
        for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
            maxSample = std::max (maxSample, audioBuffer->getMagnitude (ch, 0, length));

        if (maxSample > 0.001f) // If not silent
            audioBuffer->applyGain (0, length, 0.9f / maxSample);
    }

    const uint fadeSamples = std::min (crossFadeLength, length / 4);
    if (fadeSamples > 0)
    {
        audioBuffer->applyGainRamp (0, fadeSamples, 0.0f, 1.0f);                    // fade in
        audioBuffer->applyGainRamp (length - fadeSamples, fadeSamples, 1.0f, 0.0f); // fade out
    }

    // sync full loop copy at end of recording
    // max 5mins at 48kHz
    // - 5 × 60 × 48000 × 2 channels × 4 bytes = 110MB approx
    // Assuming 2GB/s RAM speed
    // Copy time: 110 MB / 2 GB/s = 55ms
    // Audio callback @ 512 samples:
    // 512 / 48000 = 10.7ms max callback time
    copyAudioToTmpBuffer();
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, const uint numSamples)
{
    PERFETTO_FUNCTION();
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

    if (std::abs (trackVolume - previousTrackVolume) > 0.001f)
    {
        output.applyGainRamp (0, (int) numSamples, previousTrackVolume, trackVolume);
        previousTrackVolume = trackVolume;
    }
    else
    {
        output.applyGain (trackVolume);
    }

    fifo.finishedRead (actualRead, shouldOverdub());
}

void LoopTrack::clear()
{
    PERFETTO_FUNCTION();
    audioBuffer->clear();
    undoBuffer.clear();
    tmpBuffer->clear();
    length = 0;
    provisionalLength = 0;
    fifo.prepareToPlay ((int) alignedBufferSize);
}

void LoopTrack::undo()
{
    PERFETTO_FUNCTION();
    if (! shouldOverdub() || ! isPrepared()) return;

    if (undoBuffer.undo (audioBuffer))
    {
        finalizeLayer();
    }
}

void LoopTrack::redo()
{
    PERFETTO_FUNCTION();
    if (! shouldOverdub() || ! isPrepared()) return;

    if (undoBuffer.redo (audioBuffer))
    {
        finalizeLayer();
    }
}

void LoopTrack::loadBackingTrack (const juce::AudioBuffer<float>& backingTrack)
{
    PERFETTO_FUNCTION();
    if (! isPrepared() || backingTrack.getNumChannels() != audioBuffer->getNumChannels() || backingTrack.getNumSamples() == 0) return;

    auto prevSampleRate = sampleRate;
    auto prevBlockSize = blockSize;
    auto prevChannels = channels;
    releaseResources();
    prepareToPlay (prevSampleRate, (uint) prevBlockSize, (uint) prevChannels);

    const size_t copySamples = std::min ((size_t) backingTrack.getNumSamples(), (size_t) MAX_SECONDS_HARD_LIMIT * (size_t) sampleRate);

    for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::copy (audioBuffer->getWritePointer (ch), backingTrack.getReadPointer (ch), copySamples);
    }

    provisionalLength = (size_t) copySamples;

    finalizeLayer();
}
