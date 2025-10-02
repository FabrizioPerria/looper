#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <algorithm>
#include <cassert>

//==============================================================================
// Setup
//==============================================================================

LoopTrack::LoopTrack() = default;
LoopTrack::~LoopTrack() = default;

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
    }

    fifo.prepareToPlay ((int) alignedBufferSize);
    undoBuffer.prepareToPlay ((int) maxUndoLayers, (int) numChannels, (int) alignedBufferSize);

    clear();
    setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade

    copyLoopJobs.clear();
    copyLoopJobs.reserve (MAX_POOL_SIZE);
    copyBlockJobs.clear();
    copyBlockJobs.reserve (MAX_POOL_SIZE);
    blockSnapshots.clear();
    blockSnapshots.reserve (MAX_POOL_SIZE);
    for (int i = 0; i < MAX_POOL_SIZE; ++i)
    {
        auto loopJob = std::make_unique<CopyLoopJob>();
        copyLoopJobs.push_back (std::move (loopJob));
        auto blockJob = std::make_unique<CopyInputJob>();
        copyBlockJobs.push_back (std::move (blockJob));
        blockSnapshots.push_back (new juce::AudioBuffer<float>());
        blockSnapshots.back()->setSize ((int) numChannels, (int) maxBlockSize, false, true, true);
    }

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

    static int copyJobIndexMain = 0;
    // if (copyJobIndex < 0)
    // {
    //     // no free copy job, skip this block
    //     fifo.finishedWrite (0, shouldOverdub());
    //     return;
    // }
    int copyJobIndex = copyJobIndexMain++ % MAX_POOL_SIZE;

    // copy block in temporary buffer for async processing
    for (int ch = 0; ch < input.getNumChannels(); ++ch)
        juce::FloatVectorOperations::copy (blockSnapshots[(size_t) copyJobIndex]->getWritePointer (ch),
                                           input.getReadPointer (ch),
                                           (int) numSamples);

    backgroundPool.addJob (
        [this, writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap, copyJobIndex]()
        {
            while (! tryBeginSnapshot())
                std::this_thread::yield();

            auto* src = blockSnapshots[(size_t) copyJobIndex];
            for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
            {
                if (samplesBeforeWrap > 0)
                    copyInputToLoopBuffer ((uint) ch, src->getReadPointer (ch), (uint) writePosBeforeWrap, (uint) samplesBeforeWrap);

                if (samplesAfterWrap > 0 && shouldOverdub())
                    copyInputToLoopBuffer ((uint) ch,
                                           src->getReadPointer (ch) + samplesBeforeWrap,
                                           (uint) writePosAfterWrap,
                                           (uint) samplesAfterWrap);
            }

            endSnapshot();
        });

    fifo.finishedWrite (actualWritten, shouldOverdub());

    updateLoopLength ((uint) actualWritten, (uint) audioBuffer->getNumSamples());
}

void LoopTrack::saveToUndoBuffer()
{
    if (! isPrepared() || ! shouldOverdub()) return;

    undoBuffer.pushLayer (tmpBuffer, length);
}

void LoopTrack::copyInputToLoopBuffer (const uint ch, const float* bufPtr, const uint offset, const uint numSamples)
{
    auto* audioBufferPtr = audioBuffer->getWritePointer ((int) ch) + offset;

    juce::FloatVectorOperations::multiply (audioBufferPtr, shouldOverdub() ? overdubOldGain : 0, (int) numSamples);
    juce::FloatVectorOperations::addWithMultiply (audioBufferPtr, bufPtr, overdubNewGain, (int) numSamples);
}

void LoopTrack::updateLoopLength (const uint numSamples, const uint bufferSamples)
{
    provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
}

void LoopTrack::finalizeLayer()
{
    const int currentLength = std::max ({ length, provisionalLength, 1u });
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

    // int copyJobIndex = getNextFreeCopyLoopJobIndex();
    // if (copyJobIndex < 0)
    // {
    //     // no free copy job, skip this undo layer
    //     return;
    // }
    // const int dontcare = 0;
    // copyLoopJobs[copyJobIndex]->prepare (tmpBuffer.get(),
    //                                      audioBuffer.get(),
    //                                      length,
    //                                      dontcare,
    //                                      dontcare,
    //                                      dontcare,
    //                                      dontcare,
    //                                      copyState.get(),
    //                                      shouldOverdub(),
    //                                      overdubOldGain,
    //                                      overdubNewGain);

    // backgroundPool.addJob (copyLoopJobs[copyJobIndex].get(), true);
    backgroundPool.addJob (
        [this]()
        {
            setWantLoop();

            while (! tryBeginLoop())
                std::this_thread::yield();

            for (int ch = 0; ch < tmpBuffer->getNumChannels(); ++ch)
                juce::FloatVectorOperations::copy (tmpBuffer->getWritePointer (ch), audioBuffer->getReadPointer (ch), (int) length);

            endLoop();
        });
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
    tmpBuffer->clear();
    for (auto& snapshot : blockSnapshots)
        snapshot->clear();
    copyLoopJobs.clear();
    copyBlockJobs.clear();
    blockSnapshots.clear();
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
