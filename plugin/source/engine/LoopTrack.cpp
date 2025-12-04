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

bool LoopTrack::processPlayback (juce::AudioBuffer<float>& output,
                                 const int numSamples,
                                 const bool isOverdub,
                                 const LooperState& currentLooperState)
{
    PERFETTO_FUNCTION();
    bool loopFinished = playbackEngine.processPlayback (output, bufferManager, numSamples, isOverdub);
    volumeProcessor.applyVolume (output, numSamples);
    updateUIBridge (numSamples, false, currentLooperState);
    return loopFinished;
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

void LoopTrack::loadBackingTrack (const juce::AudioBuffer<float>& backingTrack,
                                  const int masterLoopLengthSamples,
                                  const double backingTrackSampleRate)
{
    PERFETTO_FUNCTION();
    if (backingTrack.getNumChannels() != bufferManager.getNumChannels() || backingTrack.getNumSamples() == 0) return;

    auto prevSampleRate = sampleRate;
    auto prevBlockSize = blockSize;
    auto prevChannels = channels;
    releaseResources();
    prepareToPlay (prevSampleRate, (int) prevBlockSize, (int) prevChannels);

    // need to resample if backing track sample rate differs
    juce::AudioBuffer<float>& trackToUse = const_cast<juce::AudioBuffer<float>&> (backingTrack);
    juce::AudioBuffer<float> resampledBackingTrack;
    if (abs (backingTrackSampleRate - sampleRate) > 0.01)
    {
        juce::LagrangeInterpolator interpolator;
        const double ratio = backingTrackSampleRate / sampleRate;
        const int resampledLength = (int) (backingTrack.getNumSamples() / ratio) + 1;
        resampledBackingTrack.setSize (backingTrack.getNumChannels(), resampledLength);
        for (int ch = 0; ch < backingTrack.getNumChannels(); ++ch)
        {
            const float* sourceData = backingTrack.getReadPointer (ch);
            float* destData = resampledBackingTrack.getWritePointer (ch);
            interpolator.reset();
            interpolator.process (ratio, sourceData, destData, resampledLength);
        }
        trackToUse = resampledBackingTrack;
    }

    int copySamples = std::min ((int) trackToUse.getNumSamples(), (int) alignedBufferSize);
    if (isSyncedToMaster && masterLoopLengthSamples > 0)
    {
        copySamples = masterLoopLengthSamples;
    }

    bufferManager.writeToAudioBuffer ([&] (float* dest, const float* source, const int samples, const bool /*shouldOverdub*/)
                                      { juce::FloatVectorOperations::copy (dest, source, samples); },
                                      trackToUse,
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
    if (audioFile.existsAsFile())
    {
        audioFile.deleteFile();
    }

    writer.reset (wavFormat.createWriterFor (new juce::FileOutputStream (audioFile),
                                             sampleRate,
                                             (unsigned int) bufferManager.getNumChannels(),
                                             SAVE_TRACK_BITS_PER_SAMPLE,
                                             {},
                                             0));
    if (writer)
    {
        auto loopBuffer = std::make_unique<juce::AudioBuffer<float>>();
        auto length = bufferManager.getAudioBufferForSave (loopBuffer.get());
        writer->writeFromAudioSampleBuffer (*loopBuffer, 0, length);
    }
}
