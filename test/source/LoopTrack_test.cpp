#include <gtest/gtest.h>

#include "engine/LoopTrack.h"

namespace audio_plugin_test
{

// Helper function to create test buffers
juce::AudioBuffer<float> createSquareTestBuffer (int numChannels, int numSamples, double sr, float frequency)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* writePtr = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            writePtr[i] = (std::fmod ((i / sr) * frequency, 1.0) < 0.5) ? 1.0f : -1.0f;
        }
    }
    return buffer;
}

// ============================================================================
// Preparation Tests
// ============================================================================

TEST (LoopTrackPrepare, PreallocatesCorrectSize)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 120;
    const int maxBlock = 512;
    const int numChannels = 4;
    const int undoLayers = 1;
    // expected size round up to multiple of block size
    const int bufferSamples = 5292032;

    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_EQ (track.getAudioBuffer()->getNumChannels(), numChannels);
    EXPECT_EQ (track.getAudioBuffer()->getNumSamples(), bufferSamples);
    EXPECT_EQ (track.getAvailableTrackSizeSamples(), bufferSamples);
}

TEST (LoopTrackPrepare, BuffersClearedToZero)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const auto* buffer = track.getAudioBuffer();
    for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
    {
        auto* ptr = buffer->getReadPointer (ch);
        for (int i = 0; i < buffer->getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (ptr[i], 0.0f);
        }
    }
}

TEST (LoopTrackPrepare, StateReset)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;

    // Create a loop first
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    EXPECT_GT (track.getTrackLengthSamples(), 0);

    // Re-prepare should reset
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

TEST (LoopTrackPrepare, ZeroMaxSecondsDoesNotAllocate)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 0;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_EQ (track.getAudioBuffer()->getNumSamples(), 0);
}

TEST (LoopTrackPrepare, FractionalSampleRateRoundsUp)
{
    LoopTrack track;

    const double sr = 48000.1;
    const int maxSeconds = 1;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_GT (track.getAudioBuffer()->getNumSamples(), (int) sr * maxSeconds);
}

TEST (LoopTrackPrepare, LargeDurationDoesNotOverflow)
{
    LoopTrack track;

    const double sr = 44100.0;
    const int maxSeconds = 60 * 60;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_GT (track.getAudioBuffer()->getNumSamples(), 0);
    EXPECT_LT (track.getAudioBuffer()->getNumSamples(), INT_MAX);
}

TEST (LoopTrackPrepare, PrepareWithInvalidSampleRateDoesNotPrepare)
{
    LoopTrack track;
    double sr = 0.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    ASSERT_EQ (track.getAudioBuffer()->getNumSamples(), 0);

    sr = -10.0;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    ASSERT_EQ (track.getAudioBuffer()->getNumSamples(), 0);
}

// ============================================================================
// Recording Tests
// ============================================================================

TEST (LoopTrackRecord, ProcessFullBlockCopiesInput)
{
    LoopTrack track;
    const double sr = 10.0;
    const int maxSeconds = 10;
    const int maxBlock = 4;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.setOverdubGainNew (1.0f);
    track.setOverdubGainOld (1.0f);
    track.toggleNormalizingOutput(); // Disable normalization for cleaner testing

    const int numSamples = 4;
    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    auto* readPtr = input.getReadPointer (0);

    track.processRecord (input, numSamples);

    const auto* loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer->getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }

    EXPECT_EQ (track.getTrackLengthSamples(), 0);

    // process another block and check it appends correctly
    track.processRecord (input, numSamples);
    track.finalizeLayer();
    loopPtr = loopBuffer->getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i + numSamples], readPtr[i]);
    }

    EXPECT_EQ (track.getTrackLengthSamples(), numSamples * 2);
}

TEST (LoopTrackRecord, ProcessPartialBlockCopiesInput)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 1;
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    const int numSamples = 9; // less than block size
    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    auto* readPtr = input.getReadPointer (0);

    track.processRecord (input, numSamples);

    const auto* loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer->getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }
}

TEST (LoopTrackRecord, RecordingMultipleBlocks)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    const int numBlocks = 10;
    juce::AudioBuffer<float> input (numChannels, maxBlock);

    for (int block = 0; block < numBlocks; ++block)
    {
        input.clear();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = input.getWritePointer (ch);
            for (int i = 0; i < maxBlock; ++i)
            {
                data[i] = (float) block * 0.1f; // Different value per block
            }
        }

        track.processRecord (input, maxBlock);
    }

    track.finalizeLayer();

    EXPECT_EQ (track.getTrackLengthSamples(), numBlocks * maxBlock);
}

TEST (LoopTrackRecord, IsCurrentlyRecording)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_FALSE (track.isCurrentlyRecording());

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock);

    EXPECT_TRUE (track.isCurrentlyRecording());

    track.finalizeLayer();

    EXPECT_FALSE (track.isCurrentlyRecording());
}

TEST (LoopTrackRecord, ZeroSamplesDoesNothing)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    track.processRecord (input, 0);

    EXPECT_FALSE (track.isCurrentlyRecording());
    EXPECT_EQ (track.getCurrentWritePosition(), 0);
}

TEST (LoopTrackRecord, InvalidBufferDoesNothing)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    // Wrong channel count
    juce::AudioBuffer<float> wrongChannels (1, maxBlock);
    track.processRecord (wrongChannels, maxBlock);

    EXPECT_FALSE (track.isCurrentlyRecording());
}

// ============================================================================
// Playback Tests
// ============================================================================

TEST (LoopTrackPlayback, PlaybackEmptyBufferProducesNothing)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);

    // Should be silent
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = output.getReadPointer (ch);
        for (int i = 0; i < maxBlock; ++i)
        {
            EXPECT_FLOAT_EQ (data[i], 0.0f);
        }
    }
}

TEST (LoopTrackPlayback, PlaybackReproducesRecordedAudio)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record a simple pattern
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < maxBlock; ++i)
        {
            data[i] = (i % 2 == 0) ? 0.5f : -0.5f;
        }
    }

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    // Play back
    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);

    // Should have audio
    float rms = output.getRMSLevel (0, 0, maxBlock);
    EXPECT_GT (rms, 0.0f);
}

TEST (LoopTrackPlayback, LoopWrapsAround)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record short loop
    juce::AudioBuffer<float> input (numChannels, 1000);
    input.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 1000; ++i)
        {
            data[i] = 0.5f;
        }
    }

    track.processRecord (input, 1000);
    track.finalizeLayer();

    int loopLength = track.getTrackLengthSamples();
    EXPECT_EQ (loopLength, 1000);

    // Play back multiple times to force wrap
    for (int i = 0; i < 5; ++i)
    {
        juce::AudioBuffer<float> output (numChannels, maxBlock);
        output.clear();
        track.processPlayback (output, maxBlock);

        // Should always have audio
        float rms = output.getRMSLevel (0, 0, maxBlock);
        EXPECT_GT (rms, 0.0f);
    }

    // Read position should have wrapped
    int finalPos = track.getCurrentReadPosition();
    EXPECT_LT (finalPos, loopLength);
}

TEST (LoopTrackPlayback, HasWrappedAroundDetection)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 100;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record short loop
    juce::AudioBuffer<float> input (numChannels, 200);
    input.clear();
    track.processRecord (input, 200);
    track.finalizeLayer();

    // First playback - no wrap
    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);
    EXPECT_FALSE (track.hasWrappedAround());

    // Second playback - should wrap
    output.clear();
    track.processPlayback (output, maxBlock);
    EXPECT_TRUE (track.hasWrappedAround());
}

// ============================================================================
// Overdub Tests
// ============================================================================

TEST (LoopTrackOverdub, OverdubAddsToExistingLayer)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record initial layer
    juce::AudioBuffer<float> input1 (numChannels, maxBlock);
    input1.clear();
    float* data1 = input1.getWritePointer (0);
    for (int i = 0; i < maxBlock; ++i)
        data1[i] = 0.3f;

    track.processRecord (input1, maxBlock);
    track.finalizeLayer();

    int loopLength = track.getTrackLengthSamples();

    // Overdub new layer
    juce::AudioBuffer<float> input2 (numChannels, maxBlock);
    input2.clear();
    float* data2 = input2.getWritePointer (0);
    for (int i = 0; i < maxBlock; ++i)
        data2[i] = 0.2f;

    track.processRecord (input2, maxBlock);
    track.finalizeLayer();

    // Length should not change
    EXPECT_EQ (track.getTrackLengthSamples(), loopLength);

    // Play back and verify it's louder (combined)
    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);

    float rms = output.getRMSLevel (0, 0, maxBlock);
    EXPECT_GT (rms, 0.25f); // Should be higher than either input alone
}

// ============================================================================
// Undo/Redo Tests
// ============================================================================

TEST (LoopTrackUndo, UndoRestoresPreviousState)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 2;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record first layer
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    int firstLength = track.getTrackLengthSamples();

    // Record overdub
    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    // Undo should restore first length
    track.undo();

    EXPECT_EQ (track.getTrackLengthSamples(), firstLength);
}

TEST (LoopTrackUndo, UndoOnEmptyTrackDoesNothing)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.undo();

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

TEST (LoopTrackRedo, RedoRestoresUndoneState)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 2;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record two layers
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    int secondLength = track.getTrackLengthSamples();

    // Undo then redo
    track.undo();
    track.redo();

    EXPECT_EQ (track.getTrackLengthSamples(), secondLength);
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST (LoopTrackClear, ClearResetsAllState)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    // Record something
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    EXPECT_GT (track.getTrackLengthSamples(), 0);

    track.clear();

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
    EXPECT_EQ (track.getCurrentReadPosition(), 0);
    EXPECT_EQ (track.getCurrentWritePosition(), 0);
}

// ============================================================================
// Volume Tests
// ============================================================================

TEST (LoopTrackVolume, SetAndGetVolume)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.setTrackVolume (0.5f);
    EXPECT_FLOAT_EQ (track.getTrackVolume(), 0.5f);

    track.setTrackVolume (0.0f);
    EXPECT_FLOAT_EQ (track.getTrackVolume(), 0.0f);

    track.setTrackVolume (1.0f);
    EXPECT_FLOAT_EQ (track.getTrackVolume(), 1.0f);
}

TEST (LoopTrackVolume, VolumeAffectsPlayback)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record audio
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < maxBlock; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    // Play at full volume
    track.setTrackVolume (1.0f);
    juce::AudioBuffer<float> output1 (numChannels, maxBlock);
    output1.clear();
    track.processPlayback (output1, maxBlock);
    float rms1 = output1.getRMSLevel (0, 0, maxBlock);

    // Reset and play at half volume
    track.clear();
    track.processRecord (input, maxBlock);
    track.finalizeLayer();
    track.setTrackVolume (0.5f);
    juce::AudioBuffer<float> output2 (numChannels, maxBlock);
    output2.clear();
    track.processPlayback (output2, maxBlock);
    float rms2 = output2.getRMSLevel (0, 0, maxBlock);

    EXPECT_LT (rms2, rms1);
}

TEST (LoopTrackVolume, MuteAndUnmute)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_FALSE (track.isMuted());

    track.setMuted (true);
    EXPECT_TRUE (track.isMuted());
    EXPECT_FLOAT_EQ (track.getTrackVolume(), 0.0f);

    track.setMuted (false);
    EXPECT_FALSE (track.isMuted());
    EXPECT_GT (track.getTrackVolume(), 0.0f);
}

TEST (LoopTrackVolume, SoloState)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_FALSE (track.isSoloed());

    track.setSoloed (true);
    EXPECT_TRUE (track.isSoloed());

    track.setSoloed (false);
    EXPECT_FALSE (track.isSoloed());
}

// ============================================================================
// Playback Speed Tests
// ============================================================================

TEST (LoopTrackSpeed, SetAndGetPlaybackSpeed)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.setPlaybackSpeed (0.5f);
    EXPECT_FLOAT_EQ (track.getPlaybackSpeed(), 0.5f);

    track.setPlaybackSpeed (2.0f);
    EXPECT_FLOAT_EQ (track.getPlaybackSpeed(), 2.0f);

    track.setPlaybackSpeed (1.0f);
    EXPECT_FLOAT_EQ (track.getPlaybackSpeed(), 1.0f);
}

TEST (LoopTrackSpeed, SlowPlaybackWorks)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record audio
    juce::AudioBuffer<float> input (numChannels, 10000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 10000; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (input, 10000);
    track.finalizeLayer();

    // Play at half speed
    track.setPlaybackSpeed (0.5f);
    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);

    // Should have audio
    float rms = output.getRMSLevel (0, 0, maxBlock);
    EXPECT_GT (rms, 0.0f);
}

TEST (LoopTrackSpeed, FastPlaybackWorks)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record audio
    juce::AudioBuffer<float> input (numChannels, 10000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 10000; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (input, 10000);
    track.finalizeLayer();

    // Play at double speed
    track.setPlaybackSpeed (2.0f);
    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);

    // Should have audio
    float rms = output.getRMSLevel (0, 0, maxBlock);
    EXPECT_GT (rms, 0.0f);
}

TEST (LoopTrackSpeed, KeepPitchWhenChangingSpeed)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_FALSE (track.shouldKeepPitchWhenChangingSpeed());

    track.setKeepPitchWhenChangingSpeed (true);
    EXPECT_TRUE (track.shouldKeepPitchWhenChangingSpeed());

    track.setKeepPitchWhenChangingSpeed (false);
    EXPECT_FALSE (track.shouldKeepPitchWhenChangingSpeed());
}

// ============================================================================
// Playback Direction Tests
// ============================================================================

TEST (LoopTrackDirection, SetAndGetPlaybackDirection)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_TRUE (track.isPlaybackDirectionForward());

    track.setPlaybackDirectionBackward();
    EXPECT_FALSE (track.isPlaybackDirectionForward());

    track.setPlaybackDirectionForward();
    EXPECT_TRUE (track.isPlaybackDirectionForward());
}

TEST (LoopTrackDirection, ReversePlaybackWorks)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record audio
    juce::AudioBuffer<float> input (numChannels, 10000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 10000; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (input, 10000);
    track.finalizeLayer();

    // Play in reverse
    track.setPlaybackDirectionBackward();
    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);

    // Should have audio
    float rms = output.getRMSLevel (0, 0, maxBlock);
    EXPECT_GT (rms, 0.0f);
}

// ============================================================================
// Backing Track Tests
// ============================================================================

TEST (LoopTrackBacking, LoadBackingTrack)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Create backing track
    juce::AudioBuffer<float> backingTrack (numChannels, 10000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = backingTrack.getWritePointer (ch);
        for (int i = 0; i < 10000; ++i)
            data[i] = 0.5f;
    }

    track.loadBackingTrack (backingTrack);

    EXPECT_GT (track.getTrackLengthSamples(), 0);
    EXPECT_LE (track.getTrackLengthSamples(), 10000);

    // Should be able to play it back
    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);

    float rms = output.getRMSLevel (0, 0, maxBlock);
    EXPECT_GT (rms, 0.0f);
}

TEST (LoopTrackBacking, LoadBackingTrackWrongChannelsDoesNothing)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    // Wrong channel count
    juce::AudioBuffer<float> backingTrack (1, 10000);
    track.loadBackingTrack (backingTrack);

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

// ============================================================================
// Crossfade Tests
// ============================================================================

TEST (LoopTrackCrossfade, SetCrossfadeLength)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.setCrossFadeLength (1000);

    // Record and finalize to apply crossfade
    juce::AudioBuffer<float> input (numChannels, 10000);
    input.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 10000; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (input, 10000);
    track.finalizeLayer();

    // Just verify it doesn't crash
    EXPECT_GT (track.getTrackLengthSamples(), 0);
}

// ============================================================================
// Overdub Gain Tests
// ============================================================================

TEST (LoopTrackOverdubGain, SetOverdubGains)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.setOverdubGainOld (0.7f);
    track.setOverdubGainNew (1.0f);

    // Just verify it doesn't crash and can be used
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock);
    track.finalizeLayer();
}

// ============================================================================
// Position Tests
// ============================================================================

TEST (LoopTrackPosition, ReadPositionAdvances)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record
    juce::AudioBuffer<float> input (numChannels, 10000);
    input.clear();
    track.processRecord (input, 10000);
    track.finalizeLayer();

    int pos1 = track.getCurrentReadPosition();

    // Playback should advance position
    juce::AudioBuffer<float> output (numChannels, maxBlock);
    output.clear();
    track.processPlayback (output, maxBlock);

    int pos2 = track.getCurrentReadPosition();

    EXPECT_NE (pos1, pos2);
}

TEST (LoopTrackPosition, WritePositionAdvances)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    int pos1 = track.getCurrentWritePosition();

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock);

    int pos2 = track.getCurrentWritePosition();

    EXPECT_NE (pos1, pos2);
}

// ============================================================================
// Release Resources Tests
// ============================================================================

TEST (LoopTrackRelease, ReleaseResourcesClearsEverything)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    // Record something
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    track.releaseResources();

    EXPECT_EQ (track.getAudioBuffer()->getNumSamples(), 0);
    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

// ============================================================================
// Loop Duration Tests
// ============================================================================

TEST (LoopTrackDuration, GetLoopDurationSeconds)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Record 1 second of audio
    juce::AudioBuffer<float> input (numChannels, 48000);
    input.clear();
    track.processRecord (input, 48000);
    track.finalizeLayer();

    int duration = track.getLoopDurationSeconds();
    EXPECT_EQ (duration, 1);
}

} // namespace audio_plugin_test
