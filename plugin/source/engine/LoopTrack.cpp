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

    // Check if this is the start of a new recording session
    // We know we're starting recording if bufferManager needs to overdub
    // and we haven't staged anything yet
    bool isStartingNewLayer = bufferManager.shouldOverdub() && ! hasUnfinalizedRecording;

    if (isStartingNewLayer)
    {
        hasUnfinalizedRecording = true;
        undoManager.finalizeCopyAndPush (bufferManager.getLength());
    }

    // CRITICAL FIX: Don't auto-finalize when FIFO prevents wrap
    // Let the state machine handle finalization
    bool fifoPreventedWrap = bufferManager
                                 .writeToAudioBuffer ([&] (float* dest, const float* source, const int samples, const bool shouldOverdub)
                                                      { volumeProcessor.saveBalancedLayers (dest, source, samples, shouldOverdub); },
                                                      input,
                                                      numSamples,
                                                      false);

    // Just track that we hit the limit, but don't finalize
    // The LooperEngine will handle this via state transitions
    if (fifoPreventedWrap)
    {
        hitRecordingLimit = true;
    }
}

void LoopTrack::finalizeLayer()
{
    juce::Logger::outputDebugString ("LoopTrack::finalizeLayer called");
    PERFETTO_FUNCTION();

    // Reset the recording flags
    hitRecordingLimit = false;
    hasUnfinalizedRecording = false;

    bufferManager.finalizeLayer();

    auto& audioBuffer = *bufferManager.getAudioBuffer();
    auto length = bufferManager.getLength();

    // Apply audio processing
    applyPostProcessing (audioBuffer, length);

    // Stage for undo - ONLY during actual recording finalization
    undoManager.stageCurrentBuffer (audioBuffer, length);
}

void LoopTrack::applyPostProcessing (juce::AudioBuffer<float>& audioBuffer, int length)
{
    volumeProcessor.normalizeOutput (audioBuffer, length);
    volumeProcessor.applyCrossfade (audioBuffer, length);
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
    hitRecordingLimit = false;
    hasUnfinalizedRecording = false;
}

bool LoopTrack::undo()
{
    PERFETTO_FUNCTION();
    if (! bufferManager.shouldOverdub()) return false;

    // CRITICAL FIX: Don't call finalizeLayer() - it corrupts the undo stack!
    // UndoManager.undo() already swaps the buffers correctly
    if (undoManager.undo (bufferManager.getAudioBuffer()))
    {
        // Update buffer manager state to reflect the undone buffer
        bufferManager.finalizeLayer();

        // Apply audio processing to the restored buffer
        auto& audioBuffer = *bufferManager.getAudioBuffer();
        auto length = bufferManager.getLength();
        applyPostProcessing (audioBuffer, length);

        return true;
    }
    return false;
}

bool LoopTrack::redo()
{
    PERFETTO_FUNCTION();
    if (! bufferManager.shouldOverdub()) return false;

    // CRITICAL FIX: Don't call finalizeLayer() - it corrupts the undo stack!
    if (undoManager.redo (bufferManager.getAudioBuffer()))
    {
        // Update buffer manager state to reflect the redone buffer
        bufferManager.finalizeLayer();

        // Apply audio processing to the restored buffer
        auto& audioBuffer = *bufferManager.getAudioBuffer();
        auto length = bufferManager.getLength();
        applyPostProcessing (audioBuffer, length);

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
    if (! hasUnfinalizedRecording) return;

    hasUnfinalizedRecording = false;
    hitRecordingLimit = false;

    // Don't finalize - the BufferManager will handle cleanup internally
    // Just reset the recording flags
}
