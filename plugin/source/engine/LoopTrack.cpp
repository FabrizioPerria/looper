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
    if (currentSampleRate <= 0.0 || ! maxBlockSize || ! numChannels || ! maxSeconds) return;

    sampleRate = currentSampleRate;
    blockSize = (int) maxBlockSize;
    channels = (int) numChannels;
    auto requestedSamples = std::max ((int) currentSampleRate * maxSeconds, 1); // at least 1 block will be allocated
    alignedBufferSize = ((requestedSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;

    bufferManager.prepareToPlay ((int) numChannels, (int) alignedBufferSize);
    undoManager.prepareToPlay ((int) maxUndoLayers, (int) numChannels, (int) alignedBufferSize);
    volumeProcessor.prepareToPlay (sampleRate, blockSize);

    clear();

    interpolationBuffer->setSize ((int) numChannels, alignedBufferSize, false, true, true);
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
}

void LoopTrack::releaseResources()
{
    PERFETTO_FUNCTION();
    clear();
    undoManager.releaseResources();
    interpolationBuffer->setSize (0, 0, false, false, true);

    soundTouchProcessors.clear();

    sampleRate = 0.0;
    zeroBuffer.clear();

    volumeProcessor.releaseResources();
    bufferManager.releaseResources();
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
    bufferManager.getFifo().setPlaybackRate (playbackSpeed, playheadDirection);

    if (! isRecording)
    {
        isRecording = true;
        if (bufferManager.shouldOverdub()) undoManager.finalizeCopyAndPush (bufferManager.getLength());
    }

    bool fifoPreventedWrap = bufferManager
                                 .writeToAudioBuffer ([&] (float* dest, const float* source, const int samples, const bool shouldOverdub)
                                                      { volumeProcessor.saveBalancedLayers (dest, source, samples, shouldOverdub); },
                                                      input,
                                                      numSamples);
    if (fifoPreventedWrap)
    {
        finalizeLayer();
    }
}

void LoopTrack::finalizeLayer()
{
    PERFETTO_FUNCTION();
    bufferManager.finalizeLayer();
    isRecording = false;

    auto audioBuffer = *bufferManager.getAudioBuffer();
    auto length = bufferManager.getLength();
    volumeProcessor.normalizeOutput (audioBuffer, length);
    volumeProcessor.applyCrossfade (audioBuffer, length);

    undoManager.stageCurrentBuffer (audioBuffer, length);

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
            bufferManager.getFifo().setPlaybackRate (1.0f, 1);
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

    volumeProcessor.applyVolume (output, numSamples);
    wasUsingFastPath = useFastPath;
}

void LoopTrack::processPlaybackInterpolatedSpeed (juce::AudioBuffer<float>& output, const int numSamples)
{
    PERFETTO_FUNCTION();

    float speedMultiplier = playbackSpeed * (float) playheadDirection;
    bufferManager.getFifo().setPlaybackRate (playbackSpeed, playheadDirection);

    double currentPos = bufferManager.getFifo().getExactReadPos();
    int startPos = (int) currentPos;
    int maxSourceSamples = (int) ((float) numSamples * std::abs (speedMultiplier));

    copyCircularDataLinearized (startPos, maxSourceSamples, speedMultiplier, 0);

    int outputOffset = maxSourceSamples + 100;

    int channelsToProcess = (int) std::min ({ output.getNumChannels(),
                                              bufferManager.getNumChannels(),
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

    bufferManager.getFifo().finishedRead (numSamples, bufferManager.shouldOverdub());
}

void LoopTrack::processPlaybackNormalSpeedForward (juce::AudioBuffer<float>& output, const int numSamples)
{
    PERFETTO_FUNCTION();

    int readPosBeforeWrap, samplesBeforeWrap, readPosAfterWrap, samplesAfterWrap;
    bufferManager.getFifo().prepareToRead ((int) numSamples, readPosBeforeWrap, samplesBeforeWrap, readPosAfterWrap, samplesAfterWrap);
    const int actualRead = samplesBeforeWrap + samplesAfterWrap;

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        float* outPtr = output.getWritePointer (ch);
        const float* loopPtr = bufferManager.getReadPointer (ch);

        if (samplesBeforeWrap > 0) juce::FloatVectorOperations::add (outPtr, loopPtr + readPosBeforeWrap, samplesBeforeWrap);
        if (samplesAfterWrap > 0)
            juce::FloatVectorOperations::add (outPtr + samplesBeforeWrap, loopPtr + readPosAfterWrap, samplesAfterWrap);
    }

    bufferManager.getFifo().finishedRead (actualRead, bufferManager.shouldOverdub());
}

void LoopTrack::clear()
{
    PERFETTO_FUNCTION();
    undoManager.clear();
    interpolationBuffer->clear();
    playbackSpeed = 1.0f;
    playheadDirection = 1;

    for (auto& st : soundTouchProcessors)
    {
        st->clear();
    }

    volumeProcessor.clear();
    bufferManager.clear();
}

void LoopTrack::undo()
{
    PERFETTO_FUNCTION();
    if (! bufferManager.shouldOverdub()) return;

    if (undoManager.undo (bufferManager.getAudioBuffer()))
    {
        finalizeLayer();
    }
}

void LoopTrack::redo()
{
    PERFETTO_FUNCTION();
    if (! bufferManager.shouldOverdub()) return;

    if (undoManager.redo (bufferManager.getAudioBuffer()))
    {
        finalizeLayer();
    }
}

void LoopTrack::loadBackingTrack (const juce::AudioBuffer<float>& backingTrack)
{
    PERFETTO_FUNCTION();
    if (backingTrack.getNumChannels() != bufferManager.getNumChannels() || backingTrack.getNumSamples() == 0) return;

    auto prevSampleRate = sampleRate;
    auto prevBlockSize = blockSize;
    auto prevChannels = channels;
    releaseResources();
    prepareToPlay (prevSampleRate, (int) prevBlockSize, (int) prevChannels);

    const int copySamples = std::min ((int) backingTrack.getNumSamples(), (int) MAX_SECONDS_HARD_LIMIT * (int) sampleRate);

    bufferManager.writeToAudioBuffer ([&] (float* dest, const float* source, const int samples, const bool /*shouldOverdub*/)
                                      { juce::FloatVectorOperations::copy (dest, source, samples); },
                                      backingTrack,
                                      copySamples);

    finalizeLayer();
}

bool LoopTrack::shouldNotRecordInputBuffer (const juce::AudioBuffer<float>& input, const int numSamples) const
{
    return numSamples == 0 || (int) input.getNumSamples() < numSamples || input.getNumChannels() != bufferManager.getNumChannels();
}

void LoopTrack::copyCircularDataLinearized (int startPos, int numSamples, float speedMultiplier, int destOffset)
{
    PERFETTO_FUNCTION();

    bool isReverse = speedMultiplier < 0;
    int loopLen = bufferManager.getLength();

    for (int ch = 0; ch < bufferManager.getNumChannels(); ++ch)
    {
        float* destPtr = interpolationBuffer->getWritePointer (ch) + destOffset;
        const float* loopPtr = bufferManager.getReadPointer (ch);

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
