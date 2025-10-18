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

    int maxInterpSamples = (int) maxBlockSize * 2 + 20;
    interpolationBuffer->setSize ((int) numChannels, maxInterpSamples, false, true, true);

    interpolationFilters.clear();
    interpolationFilters.resize ((int) numChannels);

    clear();

    setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade

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
    interpolationBuffer->setSize (0, 0, false, false, true);

    fifo.clear();
    interpolationFilters.clear();

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

    fifo.setPlaybackRate (1.0);
    setPlaybackDirectionForward();

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
    copyAudioToTmpBuffer();
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, const uint numSamples)
{
    PERFETTO_FUNCTION();
    if (shouldNotPlayback (numSamples)) return;

    if (std::abs (playbackSpeed - 1.0f) < 0.001f)
        processPlaybackNormalSpeedForward (output, numSamples);
    else
        processPlaybackInterpolatedSpeed (output, numSamples);
    processPlaybackApplyVolume (output, numSamples);
}

void LoopTrack::processPlaybackInterpolatedSpeed (juce::AudioBuffer<float>& output, const uint numSamples)
{
    PERFETTO_FUNCTION();
    float speedMultiplier = playbackSpeed;
    if (! isPlaybackDirectionForward()) speedMultiplier *= -1.0f;

    fifo.setPlaybackRate (speedMultiplier);

    double currentPos = fifo.getExactReadPos();
    int startPos = (int) currentPos;

    // Calculate source samples needed
    int maxSourceSamples = (int) (numSamples * std::abs (speedMultiplier)) + 10;

    // Copy circular data into linear buffer - REUSED METHOD
    copyCircularDataLinearized (startPos, maxSourceSamples, speedMultiplier, 0);

    // Calculate offset for output area
    int outputOffset = (int) blockSize * 2 + 20;

    // Use interpolator
    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::clear (interpolationBuffer->getWritePointer (ch) + outputOffset, (int) numSamples);

        interpolationFilters[ch].process (std::abs (speedMultiplier),
                                          interpolationBuffer->getReadPointer (ch),
                                          interpolationBuffer->getWritePointer (ch) + outputOffset,
                                          numSamples,
                                          maxSourceSamples,
                                          0);
    }

    // Add interpolated audio to output
    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        output.addFrom (ch, 0, *interpolationBuffer, ch, outputOffset, numSamples);
    }

    fifo.finishedRead (numSamples, shouldOverdub());
}

void LoopTrack::processPlaybackApplyVolume (juce::AudioBuffer<float>& output, const uint numSamples)
{
    PERFETTO_FUNCTION();
    if (std::abs (trackVolume - previousTrackVolume) > 0.001f)
    {
        output.applyGainRamp (0, (int) numSamples, previousTrackVolume, trackVolume);
        previousTrackVolume = trackVolume;
    }
    else
    {
        output.applyGain (trackVolume);
    }
}

void LoopTrack::processPlaybackNormalSpeedForward (juce::AudioBuffer<float>& output, const uint numSamples)
{
    PERFETTO_FUNCTION();

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

bool LoopTrack::shouldNotRecordInputBuffer (const juce::AudioBuffer<float>& input, const uint numSamples) const
{
    PERFETTO_FUNCTION();
    return numSamples == 0 || (uint) input.getNumSamples() < numSamples || ! isPrepared()
           || input.getNumChannels() != audioBuffer->getNumChannels();
}

void LoopTrack::copyAudioToTmpBuffer()
{
    PERFETTO_FUNCTION();
    // Copy current audioBuffer state to tmpBuffer
    // This prepares tmpBuffer to be pushed to undo stack on next overdub
    for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::copy (tmpBuffer->getWritePointer (ch), audioBuffer->getReadPointer (ch), (int) length);
    }
}

void LoopTrack::copyCircularDataLinearized (int startPos, int numSamples, float speedMultiplier, int destOffset)
{
    PERFETTO_FUNCTION();
    // Copy circular data into linear buffer
    for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
    {
        float* destPtr = interpolationBuffer->getWritePointer (ch) + destOffset;
        const float* loopPtr = audioBuffer->getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            int readIdx = startPos + i;
            if (speedMultiplier < 0) readIdx = startPos - i;

            // Wrap around
            while (readIdx < 0)
                readIdx += length;
            while (readIdx >= (int) length)
                readIdx -= length;

            destPtr[i] = loopPtr[readIdx];
        }
    }
}
