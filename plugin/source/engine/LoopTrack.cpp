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
    if (currentSampleRate <= 0.0 || maxBlockSize <= 0 || numChannels <= 0 || maxSeconds <= 0) return;

    if (sampleRate > 0.0) releaseResources();

    sampleRate = currentSampleRate;
    blockSize = std::max (blockSize, maxBlockSize);
    channels = (int) numChannels;
    auto requestedSamples = std::max ((int) currentSampleRate * maxSeconds, 1); // at least 1 block will be allocated
    alignedBufferSize = (size_t) ((requestedSamples + blockSize - 1) / blockSize) * (size_t) blockSize;

    bufferManager.prepareToPlay ((int) numChannels, (int) alignedBufferSize);
    undoManager.prepareToPlay ((int) maxUndoLayers, (int) numChannels, (int) alignedBufferSize);
    volumeProcessor.prepareToPlay (sampleRate, blockSize);
    playbackEngine.prepareToPlay (currentSampleRate, (int) alignedBufferSize, (int) numChannels, (int) blockSize);

    clear();
}

void LoopTrack::releaseResources()
{
    PERFETTO_FUNCTION();
    clear();

    sampleRate = 0.0;

    volumeProcessor.releaseResources();
    bufferManager.releaseResources();
    playbackEngine.releaseResources();
    undoManager.releaseResources();
}

//==============================================================================
// Recording
//==============================================================================

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const int numSamples)
{
    PERFETTO_FUNCTION();
    if (shouldNotRecordInputBuffer (input, numSamples)) return;

    // playbackSpeedBeforeRecording = playbackSpeed;
    // playheadDirectionBeforeRecording = playheadDirection;

    // setPlaybackDirectionForward();
    // setPlaybackSpeed (1.0f);

    if (! isRecording)
    {
        isRecording = true;
        // bufferManager.syncReadPositionToWritePosition();
        if (bufferManager.shouldOverdub()) undoManager.finalizeCopyAndPush (bufferManager.getLength());
    }

    bool fifoPreventedWrap = bufferManager
                                 .writeToAudioBuffer ([&] (float* dest, const float* source, const int samples, const bool shouldOverdub)
                                                      { volumeProcessor.saveBalancedLayers (dest, source, samples, shouldOverdub); },
                                                      input,
                                                      numSamples,
                                                      false);
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

    auto& audioBuffer = *bufferManager.getAudioBuffer();
    auto length = bufferManager.getLength();
    volumeProcessor.normalizeOutput (audioBuffer, length);
    volumeProcessor.applyCrossfade (audioBuffer, length);

    undoManager.stageCurrentBuffer (audioBuffer, length);

    // setPlaybackSpeed (playbackSpeedBeforeRecording);
    // if (playheadDirectionBeforeRecording == 1)
    //     setPlaybackDirectionForward();
    // else
    //     setPlaybackDirectionBackward();
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, const int numSamples)
{
    PERFETTO_FUNCTION();
    playbackEngine.processPlayback (output, bufferManager, numSamples);
    volumeProcessor.applyVolume (output, numSamples);
}

void LoopTrack::clear()
{
    PERFETTO_FUNCTION();
    volumeProcessor.clear();
    bufferManager.clear();
    undoManager.clear();
    playbackEngine.clear();
}

bool LoopTrack::undo()
{
    PERFETTO_FUNCTION();
    if (! bufferManager.shouldOverdub()) return false;

    if (undoManager.undo (bufferManager.getAudioBuffer()))
    {
        finalizeLayer();
        return true;
    }
    return false;
}

bool LoopTrack::redo()
{
    PERFETTO_FUNCTION();
    if (! bufferManager.shouldOverdub()) return false;

    if (undoManager.redo (bufferManager.getAudioBuffer()))
    {
        finalizeLayer();
        return true;
    }
    return false;
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

void LoopTrack::cancelCurrentRecording()
{
    PERFETTO_FUNCTION();
    if (! isRecording) return;

    isRecording = false;

    // Don't finalize - the BufferManager will handle cleanup internally
    // Just reset the recording flag
}
