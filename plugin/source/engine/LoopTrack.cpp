#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "profiler/PerfettoProfiler.h"
#include <algorithm>
#include <cassert>

//==============================================================================
// Setup
//==============================================================================

void LoopTrack::prepareToPlay (const double currentSampleRate,
                               const int maxBlockSize,
                               const int numChannels,
                               const int maxSeconds,
                               const int maxUndoLayers)
{
    PERFETTO_FUNCTION();
    if (isPrepared() || currentSampleRate <= 0.0 || ! maxBlockSize || ! numChannels || ! maxSeconds) return;

    sampleRate = currentSampleRate;
    blockSize = (int) maxBlockSize;
    channels = (int) numChannels;
    auto requestedSamples = std::max ((int) currentSampleRate * maxSeconds, 1); // at least 1 block will be allocated
    alignedBufferSize = ((requestedSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;

    if (alignedBufferSize > (int) audioBuffer->getNumSamples())
    {
        audioBuffer->setSize ((int) numChannels, (int) alignedBufferSize, false, true, true);
        tmpBuffer->setSize ((int) numChannels, (int) alignedBufferSize, false, true, true);
        interpolationBuffer->setSize ((int) numChannels, alignedBufferSize, false, true, true);
    }

    undoBuffer.prepareToPlay ((int) maxUndoLayers, (int) numChannels, (int) alignedBufferSize);

    clear();

    soundTouchProcessors.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        soundTouchProcessors.push_back (std::make_unique<soundtouch::SoundTouch>());
    }

    for (auto& soundTouchProcessor : soundTouchProcessors)
    {
        soundTouchProcessor->setSampleRate ((uint) sampleRate);
        soundTouchProcessor->setChannels (1);
        soundTouchProcessor->setPitchSemiTones (0);
        soundTouchProcessor->setSetting (SETTING_USE_QUICKSEEK, 0);
        soundTouchProcessor->setSetting (SETTING_USE_AA_FILTER, 1);
        soundTouchProcessor->setSetting (SETTING_SEQUENCE_MS, 82);
        soundTouchProcessor->setSetting (SETTING_SEEKWINDOW_MS, 28);
        soundTouchProcessor->setSetting (SETTING_OVERLAP_MS, 12);
    }

    zeroBuffer.resize ((size_t) blockSize);
    std::fill (zeroBuffer.begin(), zeroBuffer.end(), 0.0f);

    setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade

    previousReadPos = -1.0;
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
    soundTouchProcessors.clear();

    sampleRate = 0.0;
    alreadyPrepared = false;
    zeroBuffer.clear();
}

//==============================================================================
// Recording
//==============================================================================

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const int numSamples)
{
    PERFETTO_FUNCTION();
    if (shouldNotRecordInputBuffer (input, numSamples)) return;

    playbackSpeedBeforeRecording = playbackSpeed;
    playheadDirectionBeforeRecording = playheadDirection;

    setPlaybackDirectionForward();
    setPlaybackSpeed (1.0f);
    fifo.setPlaybackRate (playbackSpeed, playheadDirection);

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
            copyInputToLoopBuffer ((int) ch, input.getReadPointer (ch), (int) writePosBeforeWrap, (int) samplesBeforeWrap);
        }
        if (samplesAfterWrap > 0 && shouldOverdub())
        {
            copyInputToLoopBuffer ((int) ch,
                                   input.getReadPointer (ch) + samplesBeforeWrap,
                                   (int) writePosAfterWrap,
                                   (int) samplesAfterWrap);
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

    updateLoopLength ((int) samplesBeforeWrap, shouldOverdub() ? length : (int) audioBuffer->getNumSamples());
}

void LoopTrack::saveToUndoBuffer()
{
    PERFETTO_FUNCTION();
    if (! isPrepared() || ! shouldOverdub()) return;

    undoBuffer.finalizeCopyAndPush (tmpBuffer, length);
}

void LoopTrack::copyInputToLoopBuffer (const int ch, const float* bufPtr, const int offset, const int numSamples)
{
    if (! isRecording) return;

    auto* dest = audioBuffer->getWritePointer ((int) ch) + offset;

    juce::FloatVectorOperations::multiply (dest, shouldOverdub() ? (float) overdubOldGain : 0, (int) numSamples);
    juce::FloatVectorOperations::addWithMultiply (dest, bufPtr, (float) overdubNewGain, (int) numSamples);
}

void LoopTrack::updateLoopLength (const int numSamples, const int bufferSamples)
{
    provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
}

void LoopTrack::finalizeLayer()
{
    PERFETTO_FUNCTION();
    const int currentLength = std::max ({ (int) length, (int) provisionalLength, 1 });
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

    const int fadeSamples = std::min (crossFadeLength, length / 4);
    if (fadeSamples > 0)
    {
        audioBuffer->applyGainRamp (0, fadeSamples, 0.0f, 1.0f);                    // fade in
        audioBuffer->applyGainRamp (length - fadeSamples, fadeSamples, 1.0f, 0.0f); // fade out
    }

    // sync full loop copy at end of recording
    copyAudioToTmpBuffer();

    setPlaybackSpeed (playbackSpeedBeforeRecording);
    if (playheadDirectionBeforeRecording == 1)
        setPlaybackDirectionForward();
    else
        setPlaybackDirectionBackward();
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, const int numSamples)
{
    PERFETTO_FUNCTION();
    if (shouldNotPlayback (numSamples)) return;

    bool useFastPath = (std::abs (playbackSpeed - 1.0f) < 0.01f && isPlaybackDirectionForward());

    if (useFastPath)
    {
        if (! wasUsingFastPath)
        {
            for (auto& st : soundTouchProcessors)
            {
                st->setRate (1.0);
                st->setTempo (1.0);
                st->setPitchSemiTones (0);
            }
            fifo.setPlaybackRate (1.0f, 1);
        }

        int channelsToFeed = std::min ((int) soundTouchProcessors.size(), output.getNumChannels());
        for (int ch = 0; ch < channelsToFeed; ++ch)
        {
            soundTouchProcessors[ch]->putSamples (zeroBuffer.data(), (uint) numSamples);

            while (soundTouchProcessors[ch]->numSamples() > (uint) (numSamples * 2))
            {
                soundTouchProcessors[ch]->receiveSamples (zeroBuffer.data(), (uint) numSamples);
            }
        }

        processPlaybackNormalSpeedForward (output, numSamples);
    }
    else
    {
        processPlaybackInterpolatedSpeed (output, numSamples);
    }

    wasUsingFastPath = useFastPath;
    processPlaybackApplyVolume (output, numSamples);
}

void LoopTrack::processPlaybackInterpolatedSpeed (juce::AudioBuffer<float>& output, const int numSamples)
{
    PERFETTO_FUNCTION();

    float speedMultiplier = playbackSpeed * (float) playheadDirection;
    fifo.setPlaybackRate (playbackSpeed, playheadDirection);

    double currentPos = fifo.getExactReadPos();
    int startPos = (int) currentPos;
    int maxSourceSamples = (int) ((float) numSamples * std::abs (speedMultiplier));

    copyCircularDataLinearized (startPos, maxSourceSamples, speedMultiplier, 0);

    int outputOffset = maxSourceSamples + 100;

    int channelsToProcess = (int) std::min ({ output.getNumChannels(),
                                              audioBuffer->getNumChannels(),
                                              interpolationBuffer->getNumChannels(),
                                              (int) soundTouchProcessors.size() });

    bool speedChanged = std::abs (speedMultiplier - previousPlaybackSpeed) > 0.001f;
    bool modeChanged = (shouldKeepPitchWhenChangingSpeed() != previousKeepPitch);

    previousPlaybackSpeed = speedMultiplier;
    previousKeepPitch = shouldKeepPitchWhenChangingSpeed();

    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        auto& st = soundTouchProcessors[(size_t) ch];

        if (speedChanged || modeChanged)
        {
            if (shouldKeepPitchWhenChangingSpeed())
            {
                st->setRate (1.0);            // Reset rate to neutral
                st->setTempo (playbackSpeed); // Control speed via tempo
                st->setPitchSemiTones (0);    // Ensure no pitch shift
            }
            else
            {
                st->setTempo (1.0);          // Reset tempo to neutral
                st->setRate (playbackSpeed); // Control speed via rate
                st->setPitchSemiTones (0);   // Ensure no pitch shift
            }
        }

        // Read from FIRST HALF
        st->putSamples (interpolationBuffer->getReadPointer (ch), (uint) maxSourceSamples);
        while (st->numSamples() < (uint) numSamples)
        {
            st->putSamples (interpolationBuffer->getReadPointer (ch), (uint) maxSourceSamples);
        }

        uint received = st->receiveSamples (interpolationBuffer->getWritePointer (ch) + outputOffset, (uint) numSamples);

        if (received < (uint) numSamples)
            juce::FloatVectorOperations::clear (interpolationBuffer->getWritePointer (ch) + outputOffset + received,
                                                (int) ((uint) numSamples - received));

        output.addFrom (ch, 0, *interpolationBuffer, ch, outputOffset, (int) numSamples);
    }

    fifo.finishedRead (numSamples, shouldOverdub());
}

void LoopTrack::processPlaybackApplyVolume (juce::AudioBuffer<float>& output, const int numSamples)
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

void LoopTrack::processPlaybackNormalSpeedForward (juce::AudioBuffer<float>& output, const int numSamples)
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
    interpolationBuffer->clear();
    length = 0;
    provisionalLength = 0;
    playbackSpeed = 1.0f;
    playheadDirection = 1;
    fifo.prepareToPlay ((int) alignedBufferSize);
    for (auto& st : soundTouchProcessors)
    {
        st->clear();
    }
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
    prepareToPlay (prevSampleRate, (int) prevBlockSize, (int) prevChannels);

    const int copySamples = std::min ((int) backingTrack.getNumSamples(), (int) MAX_SECONDS_HARD_LIMIT * (int) sampleRate);

    for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::copy (audioBuffer->getWritePointer (ch), backingTrack.getReadPointer (ch), copySamples);
    }

    provisionalLength = (int) copySamples;

    finalizeLayer();
}

bool LoopTrack::shouldNotRecordInputBuffer (const juce::AudioBuffer<float>& input, const int numSamples) const
{
    PERFETTO_FUNCTION();
    return numSamples == 0 || (int) input.getNumSamples() < numSamples || ! isPrepared()
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

    bool isReverse = speedMultiplier < 0;
    int loopLen = (int) length;

    for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
    {
        float* destPtr = interpolationBuffer->getWritePointer (ch) + destOffset;
        const float* loopPtr = audioBuffer->getReadPointer (ch);

        if (! isReverse)
        {
            int remaining = numSamples;
            int srcPos = startPos % loopLen;
            int destPos = 0;

            while (remaining > 0)
            {
                int chunkSize = std::min (remaining, loopLen - srcPos);
                juce::FloatVectorOperations::copy (destPtr + destPos, loopPtr + srcPos, chunkSize);

                destPos += chunkSize;
                srcPos = (srcPos + chunkSize) % loopLen;
                remaining -= chunkSize;
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                int readIdx = startPos - i;
                while (readIdx < 0)
                    readIdx += loopLen;
                destPtr[i] = loopPtr[readIdx % loopLen];
            }
        }
    }
}
