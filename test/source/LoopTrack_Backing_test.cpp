#include "engine/LoopTrack.h"
#include <gtest/gtest.h>

namespace audio_plugin_test
{

class LoopTrackBackingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        track.prepareToPlay (sampleRate, maxBlockSize, numChannels, maxSeconds, undoLayers);
        track.setCrossFadeLength (0);
    }

    LoopTrack track;
    const double sampleRate = 48000.0;
    const int maxBlockSize = 512;
    const int numChannels = 2;
    const int maxSeconds = 10 * 60;
    const int undoLayers = 1;
};

// ============================================================================
// Backing Track Tests
// ============================================================================

TEST_F (LoopTrackBackingTest, LoadBackingTrack)
{
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
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackBackingTest, LoadBackingTrackWrongChannelsDoesNothing)
{
    // Wrong channel count
    juce::AudioBuffer<float> backingTrack (1, 10000);
    backingTrack.clear();

    track.loadBackingTrack (backingTrack);

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

TEST_F (LoopTrackBackingTest, LoadBackingTrackEmptyDoesNothing)
{
    // Empty buffer
    juce::AudioBuffer<float> backingTrack (numChannels, 0);

    track.loadBackingTrack (backingTrack);

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

TEST_F (LoopTrackBackingTest, LoadBackingTrackReplacesExisting)
{
    // Record something first
    juce::AudioBuffer<float> input (numChannels, 1000);
    input.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 1000; ++i)
            data[i] = 0.3f;
    }
    track.processRecord (input, 1000, false);
    track.finalizeLayer (false);

    // Load backing track
    juce::AudioBuffer<float> backingTrack (numChannels, 5000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = backingTrack.getWritePointer (ch);
        for (int i = 0; i < 5000; ++i)
            data[i] = 0.7f;
    }

    track.loadBackingTrack (backingTrack);

    // Should have new length
    EXPECT_EQ (track.getTrackLengthSamples(), 5000);

    // Play back and verify different amplitude
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.5f); // Should be louder than 0.3
}

TEST_F (LoopTrackBackingTest, LoadBackingTrackPreservesMultiChannel)
{
    // Create backing track with different content per channel
    juce::AudioBuffer<float> backingTrack (numChannels, 5000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = backingTrack.getWritePointer (ch);
        for (int i = 0; i < 5000; ++i)
            data[i] = 0.5f * (ch + 1); // Different amplitude per channel
    }

    track.loadBackingTrack (backingTrack);

    // Play back
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    // Check that channels are different
    float rms0 = output.getRMSLevel (0, 0, maxBlockSize);
    float rms1 = output.getRMSLevel (1, 0, maxBlockSize);

    EXPECT_GT (rms0, 0.0f);
    EXPECT_GT (rms1, 0.0f);
    EXPECT_NE (rms0, rms1);
}

TEST_F (LoopTrackBackingTest, LoadBackingTrackAllowsOverdub)
{
    // Load backing track
    juce::AudioBuffer<float> backingTrack (numChannels, 5000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = backingTrack.getWritePointer (ch);
        for (int i = 0; i < 5000; ++i)
            data[i] = 0.3f;
    }
    track.loadBackingTrack (backingTrack);

    // Get initial RMS
    juce::AudioBuffer<float> output1 (numChannels, maxBlockSize);
    output1.clear();
    track.processPlayback (output1, maxBlockSize, false);
    float rms1 = output1.getRMSLevel (0, 0, maxBlockSize);

    // Overdub on top
    juce::AudioBuffer<float> overdub (numChannels, 5000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = overdub.getWritePointer (ch);
        for (int i = 0; i < 5000; ++i)
            data[i] = 0.2f;
    }
    track.processRecord (overdub, 5000, true);
    track.finalizeLayer (true);

    // Should be louder now
    juce::AudioBuffer<float> output2 (numChannels, maxBlockSize);
    output2.clear();
    track.processPlayback (output2, maxBlockSize, false);
    float rms2 = output2.getRMSLevel (0, 0, maxBlockSize);

    EXPECT_GT (rms2, rms1);
}

TEST_F (LoopTrackBackingTest, LoadVeryLongBackingTrackTruncates)
{
    // Try to load more than max seconds allows
    int hugeSize = (int) sampleRate * (maxSeconds + 10); // 10 seconds over limit
    juce::AudioBuffer<float> backingTrack (numChannels, hugeSize);
    backingTrack.clear();

    track.loadBackingTrack (backingTrack);

    // Should be truncated to max allowed
    EXPECT_LE (track.getTrackLengthSamples(), (int) sampleRate * maxSeconds);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F (LoopTrackBackingTest, RecordAndPlaybackSingleSample)
{
    // Extreme case: single sample
    juce::AudioBuffer<float> input (numChannels, 1);
    for (int ch = 0; ch < numChannels; ++ch)
        input.setSample (ch, 0, 0.5f);

    track.processRecord (input, 1, false);
    track.finalizeLayer (false);

    EXPECT_EQ (track.getTrackLengthSamples(), 1);

    // Playback should work (though it will immediately loop)
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    // Should have non-zero audio (repeated)
    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackBackingTest, VeryShortLoop)
{
    // Record 10 samples
    juce::AudioBuffer<float> input (numChannels, 10);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 10; ++i)
            data[i] = 0.5f;
    }

    track.processRecord (input, 10, false);
    track.finalizeLayer (false);

    // Play back much more than loop length
    for (int i = 0; i < 100; ++i)
    {
        juce::AudioBuffer<float> output (numChannels, maxBlockSize);
        output.clear();
        track.processPlayback (output, maxBlockSize, false);

        // Should keep working
        float rms = output.getRMSLevel (0, 0, maxBlockSize);
        EXPECT_GT (rms, 0.0f);
    }
}

TEST_F (LoopTrackBackingTest, RecordExactlyBufferSize)
{
    int bufferSize = track.getAvailableTrackSizeSamples();

    // Record exactly the full buffer
    juce::AudioBuffer<float> input (numChannels, bufferSize);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < bufferSize; ++i)
            data[i] = 0.5f;
    }

    track.processRecord (input, bufferSize, false);
    track.finalizeLayer (false);

    EXPECT_EQ (track.getTrackLengthSamples(), bufferSize);

    // Should play back
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackBackingTest, AlternatingRecordAndPlayback)
{
    // Record
    juce::AudioBuffer<float> input (numChannels, 1000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 1000; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (input, 1000, false);
    track.finalizeLayer (false);

    // Playback
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);
    EXPECT_GT (output.getRMSLevel (0, 0, maxBlockSize), 0.0f);

    // Overdub
    track.processRecord (input, 1000, true);
    track.finalizeLayer (true);

    // Playback again
    output.clear();
    track.processPlayback (output, maxBlockSize, false);
    EXPECT_GT (output.getRMSLevel (0, 0, maxBlockSize), 0.0f);
}

TEST_F (LoopTrackBackingTest, NegativeInputSamples)
{
    // Record with negative amplitude
    juce::AudioBuffer<float> input (numChannels, 1000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 1000; ++i)
            data[i] = -0.5f;
    }
    track.processRecord (input, 1000, false);
    track.finalizeLayer (false);

    // Should play back correctly
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);

    // Check that we actually have negative samples
    bool hasNegative = false;
    for (int ch = 0; ch < numChannels && ! hasNegative; ++ch)
    {
        const float* data = output.getReadPointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
        {
            if (data[i] < 0.0f)
            {
                hasNegative = true;
                break;
            }
        }
    }
    EXPECT_TRUE (hasNegative);
}

TEST_F (LoopTrackBackingTest, SilentInputCreatesLoop)
{
    // Record silent audio
    juce::AudioBuffer<float> input (numChannels, 1000);
    input.clear();

    track.processRecord (input, 1000, false);
    track.finalizeLayer (false);

    EXPECT_EQ (track.getTrackLengthSamples(), 1000);

    // Playback should be silent
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_FLOAT_EQ (rms, 0.0f);
}

TEST_F (LoopTrackBackingTest, FullScaleInput)
{
    // Record at full scale
    juce::AudioBuffer<float> input (numChannels, 1000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 1000; ++i)
            data[i] = 1.0f;
    }
    track.processRecord (input, 1000, false);
    track.finalizeLayer (false);

    // Should normalize and not clip
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    // Check no samples exceed 1.0
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = output.getReadPointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
        {
            EXPECT_LE (std::abs (data[i]), 1.0f);
        }
    }
}

} // namespace audio_plugin_test
