#include "LoopTrack.h"
#include "engine/Constants.h"
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

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input,
                               const int numSamples,
                               const bool isOverdub,
                               const LooperState& currentLooperState)
{
    PERFETTO_FUNCTION();

    bufferManager.writeToAudioBuffer ([&] (float* dest, const float* source, const int samples, const bool shouldOverdub)
                                      { volumeProcessor.saveBalancedLayers (dest, source, samples, shouldOverdub); },
                                      input,
                                      numSamples,
                                      isOverdub,
                                      true);
    updateUIBridge (numSamples, true, currentLooperState);
}

void LoopTrack::initializeForNewOverdubSession()
{
    PERFETTO_FUNCTION();
    undoManager.finalizeCopyAndPush (bufferManager.getLength());
}

void LoopTrack::finalizeLayer (const bool isOverdub, const int masterLoopLengthSamples)
{
    juce::Logger::outputDebugString ("LoopTrack::finalizeLayer called");
    PERFETTO_FUNCTION();

    bufferManager.finalizeLayer (isOverdub, masterLoopLengthSamples);

    auto& audioBuffer = *bufferManager.getAudioBuffer();
    auto length = bufferManager.getLength();

    applyPostProcessing (audioBuffer, length);
    undoManager.stageCurrentBuffer (audioBuffer, length);
    uiBridge->signalWaveformChanged();
}

void LoopTrack::applyPostProcessing (juce::AudioBuffer<float>& audioBuffer, int length)
{
    volumeProcessor.normalizeOutput (audioBuffer, length);
    volumeProcessor.applyCrossfade (audioBuffer, length);
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output,
                                 const int numSamples,
                                 const bool isOverdub,
                                 const LooperState& currentLooperState)
{
    PERFETTO_FUNCTION();
    playbackEngine.processPlayback (output, bufferManager, numSamples, isOverdub);
    volumeProcessor.applyVolume (output, numSamples);
    updateUIBridge (numSamples, false, currentLooperState);
}

void LoopTrack::clear()
{
    PERFETTO_FUNCTION();
    volumeProcessor.clear();
    bufferManager.clear();
    undoManager.clear();
    playbackEngine.clear();

    uiBridge->clear();
    bridgeInitialized = false;
    updateUIBridge (0, false, LooperState::Stopped);
}

void LoopTrack::resetPlaybackPosition (LooperState currentState)
{
    PERFETTO_FUNCTION();
    bufferManager.fromScratch();
    updateUIBridge (bufferManager.getLength(), false, currentState);
}

bool LoopTrack::undo()
{
    PERFETTO_FUNCTION();
    if (bufferManager.getLength() == 0) return false;

    if (undoManager.undo (bufferManager.getAudioBuffer()))
    {
        bufferManager.finalizeLayer (true, 0);

        auto& audioBuffer = *bufferManager.getAudioBuffer();
        auto length = bufferManager.getLength();
        applyPostProcessing (audioBuffer, length);
        undoManager.stageCurrentBuffer (audioBuffer, length);

        uiBridge->signalWaveformChanged();
        return true;
    }
    return false;
}

bool LoopTrack::redo()
{
    PERFETTO_FUNCTION();
    if (bufferManager.getLength() == 0) return false;

    if (undoManager.redo (bufferManager.getAudioBuffer()))
    {
        bufferManager.finalizeLayer (true, 0);

        auto& audioBuffer = *bufferManager.getAudioBuffer();
        auto length = bufferManager.getLength();
        applyPostProcessing (audioBuffer, length);
        undoManager.stageCurrentBuffer (audioBuffer, length);

        uiBridge->signalWaveformChanged();
        return true;
    }
    return false;
}

void LoopTrack::loadBackingTrack (const juce::AudioBuffer<float>& backingTrack, const int masterLoopLengthSamples)
{
    PERFETTO_FUNCTION();
    if (backingTrack.getNumChannels() != bufferManager.getNumChannels() || backingTrack.getNumSamples() == 0) return;

    auto prevSampleRate = sampleRate;
    auto prevBlockSize = blockSize;
    auto prevChannels = channels;
    releaseResources();
    prepareToPlay (prevSampleRate, (int) prevBlockSize, (int) prevChannels);

    int copySamples = std::min ((int) backingTrack.getNumSamples(), (int) LOOP_MAX_SECONDS_HARD_LIMIT * (int) sampleRate);
    if (isSyncedToMaster && masterLoopLengthSamples > 0)
    {
        copySamples = masterLoopLengthSamples;
    }

    bufferManager.writeToAudioBuffer ([&] (float* dest, const float* source, const int samples, const bool /*shouldOverdub*/)
                                      { juce::FloatVectorOperations::copy (dest, source, samples); },
                                      backingTrack,
                                      copySamples,
                                      false,
                                      false);

    finalizeLayer (false, copySamples);
    updateUIBridge (copySamples, false, LooperState::Stopped);
}

void LoopTrack::saveTrackToWavFile (const juce::File& audioFile)
{
    PERFETTO_FUNCTION();
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatWriter> writer;

    juce::WavAudioFormat wavFormat;
    writer.reset (wavFormat.createWriterFor (new juce::FileOutputStream (audioFile),
                                             sampleRate,
                                             (unsigned int) bufferManager.getNumChannels(),
                                             SAVE_TRACK_BITS_PER_SAMPLE,
                                             {},
                                             0));
    if (writer)
    {
        auto& audioBuffer = *bufferManager.getAudioBuffer();
        auto length = bufferManager.getLength();
        writer->writeFromAudioSampleBuffer (audioBuffer, 0, length);
    }
}
