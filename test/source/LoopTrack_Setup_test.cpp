#include "engine/LoopTrack.h"
#include <gtest/gtest.h>

namespace audio_plugin_test
{

// ============================================================================
// Preparation and Configuration Tests
// ============================================================================

class LoopTrackSetupTest : public ::testing::Test
{
protected:
    LoopTrack track;
};

TEST_F (LoopTrackSetupTest, PreallocatesCorrectSize)
{
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

TEST_F (LoopTrackSetupTest, BuffersClearedToZero)
{
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

TEST_F (LoopTrackSetupTest, StateReset)
{
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;

    // Create a loop first
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock, false);
    track.finalizeLayer (false);

    EXPECT_GT (track.getTrackLengthSamples(), 0);

    // Re-prepare should reset
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

TEST_F (LoopTrackSetupTest, ZeroMaxSecondsDoesNotAllocate)
{
    const double sr = 44100.0;
    const int maxSeconds = 0;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_EQ (track.getAudioBuffer()->getNumSamples(), 0);
}

TEST_F (LoopTrackSetupTest, FractionalSampleRateRoundsUp)
{
    const double sr = 48000.1;
    const int maxSeconds = 1;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_GT (track.getAudioBuffer()->getNumSamples(), (int) sr * maxSeconds);
}

TEST_F (LoopTrackSetupTest, LargeDurationDoesNotOverflow)
{
    const double sr = 44100.0;
    const int maxSeconds = 60 * 60;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_GT (track.getAudioBuffer()->getNumSamples(), 0);
    EXPECT_LT (track.getAudioBuffer()->getNumSamples(), INT_MAX);
}

TEST_F (LoopTrackSetupTest, PrepareWithInvalidSampleRateDoesNotPrepare)
{
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

TEST_F (LoopTrackSetupTest, PrepareWithInvalidBlockSizeDoesNotPrepare)
{
    const double sr = 44100.0;
    const int maxSeconds = 10;
    int maxBlock = 0;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    ASSERT_EQ (track.getAudioBuffer()->getNumSamples(), 0);

    maxBlock = -512;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    ASSERT_EQ (track.getAudioBuffer()->getNumSamples(), 0);
}

TEST_F (LoopTrackSetupTest, PrepareWithInvalidChannelCountDoesNotPrepare)
{
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    int numChannels = 0;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    ASSERT_EQ (track.getAudioBuffer()->getNumSamples(), 0);

    numChannels = -2;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    ASSERT_EQ (track.getAudioBuffer()->getNumSamples(), 0);
}

TEST_F (LoopTrackSetupTest, ReprepareWithLargerBlockGrowsBuffer)
{
    const double sr = 44100.0;
    const int maxSeconds = 10;
    int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    int firstSize = track.getAudioBuffer()->getNumSamples();

    // simulate host requesting a bigger block
    maxBlock = 1024;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    int secondSize = track.getAudioBuffer()->getNumSamples();

    EXPECT_GE (secondSize, firstSize);
}

TEST_F (LoopTrackSetupTest, ReprepareWithSmallerBlockKeepsBufferSize)
{
    const double sr = 44100.0;
    const int maxSeconds = 10;
    int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    int firstSize = track.getAudioBuffer()->getNumSamples();

    // simulate host requesting a smaller block
    maxBlock = 256;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    int secondSize = track.getAudioBuffer()->getNumSamples();

    EXPECT_EQ (secondSize, firstSize);
}

// ============================================================================
// Volume and Mixing Configuration Tests
// ============================================================================

TEST_F (LoopTrackSetupTest, SetAndGetVolume)
{
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

TEST_F (LoopTrackSetupTest, MuteAndUnmute)
{
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

TEST_F (LoopTrackSetupTest, SoloState)
{
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

TEST_F (LoopTrackSetupTest, SetCrossfadeLength)
{
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
    track.processRecord (input, 10000, false);
    track.finalizeLayer (false);

    // Just verify it doesn't crash
    EXPECT_GT (track.getTrackLengthSamples(), 0);
}

TEST_F (LoopTrackSetupTest, SetOverdubGains)
{
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.setOverdubGainOld (0.7f);
    track.setOverdubGainNew (1.0f);
    track.toggleNormalizingOutput();

    // Just verify it doesn't crash and can be used
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock, false);
    track.finalizeLayer (false);
}

// ============================================================================
// Resource Management Tests
// ============================================================================

TEST_F (LoopTrackSetupTest, ReleaseResourcesClearsEverything)
{
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    // Record something
    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    track.processRecord (input, maxBlock, false);
    track.finalizeLayer (false);

    track.releaseResources();

    EXPECT_EQ (track.getAudioBuffer()->getNumSamples(), 0);
    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

TEST_F (LoopTrackSetupTest, ReleaseAndReprepare)
{
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;

    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.releaseResources();
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_GT (track.getAudioBuffer()->getNumSamples(), 0);
}

// ============================================================================
// Query Methods Tests
// ============================================================================

TEST_F (LoopTrackSetupTest, GetLoopDurationSeconds)
{
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
    track.processRecord (input, 48000, false);
    track.finalizeLayer (false);

    int duration = track.getLoopDurationSeconds();
    EXPECT_EQ (duration, 1);
}

TEST_F (LoopTrackSetupTest, GetAvailableTrackSize)
{
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    int availableSize = track.getAvailableTrackSizeSamples();
    EXPECT_GT (availableSize, 0);
    EXPECT_GE (availableSize, (int) sr * maxSeconds);
}

} // namespace audio_plugin_test
