#include "engine/LoopTrack.h"
#include <gtest/gtest.h>

namespace audio_plugin_test
{

class LoopTrackPlaybackTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        track.prepareToPlay (sampleRate, maxBlockSize, numChannels, maxSeconds, undoLayers);
        track.setCrossFadeLength (0);
    }

    void recordTestLoop (int samples, float amplitude, bool isOverdub)
    {
        juce::AudioBuffer<float> input (numChannels, samples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = input.getWritePointer (ch);
            for (int i = 0; i < samples; ++i)
                data[i] = amplitude;
        }
        track.processRecord (input, samples, isOverdub);
        track.finalizeLayer (isOverdub);
    }

    LoopTrack track;
    const double sampleRate = 48000.0;
    const int maxBlockSize = 512;
    const int numChannels = 2;
    const int maxSeconds = 10;
    const int undoLayers = 1;
};

TEST_F (LoopTrackPlaybackTest, PlaybackEmptyBufferProducesSilence)
{
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    // Should be silent
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = output.getReadPointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
        {
            EXPECT_FLOAT_EQ (data[i], 0.0f);
        }
    }
}

TEST_F (LoopTrackPlaybackTest, PlaybackReproducesRecordedAudio)
{
    // Record a simple pattern
    recordTestLoop (maxBlockSize, 0.5f, false);

    // Play back
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    // Should have audio
    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackPlaybackTest, LoopWrapsAround)
{
    // Record short loop
    recordTestLoop (1000, 0.5f, false);

    int loopLength = track.getTrackLengthSamples();
    EXPECT_EQ (loopLength, 1000);

    // Play back multiple times to force wrap
    for (int i = 0; i < 5; ++i)
    {
        juce::AudioBuffer<float> output (numChannels, maxBlockSize);
        output.clear();
        track.processPlayback (output, maxBlockSize, false);

        // Should always have audio
        float rms = output.getRMSLevel (0, 0, maxBlockSize);
        EXPECT_GT (rms, 0.0f);
    }

    // Read position should have wrapped
    int finalPos = track.getCurrentReadPosition();
    EXPECT_LT (finalPos, loopLength);
}

TEST_F (LoopTrackPlaybackTest, HasWrappedAroundDetection)
{
    // Record short loop
    recordTestLoop (200, 0.5f, false);

    // First playback - no wrap
    juce::AudioBuffer<float> output (numChannels, 100);
    output.clear();
    track.processPlayback (output, 100, false);
    EXPECT_FALSE (track.hasWrappedAround());

    // Second playback - should wrap
    output.clear();
    track.processPlayback (output, 100, false);
    EXPECT_TRUE (track.hasWrappedAround());
}

TEST_F (LoopTrackPlaybackTest, ReadPositionAdvances)
{
    recordTestLoop (10000, 0.5f, false);

    int pos1 = track.getCurrentReadPosition();

    // Playback should advance position
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    int pos2 = track.getCurrentReadPosition();

    EXPECT_NE (pos1, pos2);
}

TEST_F (LoopTrackPlaybackTest, ContinuousPlaybackMaintainsQuality)
{
    recordTestLoop (10000, 0.5f, false);

    // Play for many cycles
    for (int i = 0; i < 100; ++i)
    {
        juce::AudioBuffer<float> output (numChannels, maxBlockSize);
        output.clear();
        track.processPlayback (output, maxBlockSize, false);

        // Audio should remain consistent
        float rms = output.getRMSLevel (0, 0, maxBlockSize);
        EXPECT_GT (rms, 0.0f);
        EXPECT_LT (rms, 1.0f); // No clipping
    }
}

TEST_F (LoopTrackPlaybackTest, PlaybackWithZeroSamplesDoesNothing)
{
    recordTestLoop (1000, 0.5f, false);

    int posBefore = track.getCurrentReadPosition();

    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    track.processPlayback (output, 0, false); // Zero samples

    int posAfter = track.getCurrentReadPosition();

    EXPECT_EQ (posBefore, posAfter);
}

TEST_F (LoopTrackPlaybackTest, MultipleChannelsPlaybackCorrectly)
{
    // Record different content per channel
    juce::AudioBuffer<float> input (numChannels, 1000);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 1000; ++i)
            data[i] = 0.5f * (ch + 1); // Different amplitude per channel
    }
    track.processRecord (input, 1000, false);
    track.finalizeLayer (false);

    // Play back
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    // Both channels should have different but non-zero audio
    float rms0 = output.getRMSLevel (0, 0, maxBlockSize);
    float rms1 = output.getRMSLevel (1, 0, maxBlockSize);

    EXPECT_GT (rms0, 0.0f);
    EXPECT_GT (rms1, 0.0f);
    EXPECT_NE (rms0, rms1); // Different content
}

TEST_F (LoopTrackPlaybackTest, PlaybackAfterClearIsSilent)
{
    recordTestLoop (1000, 0.5f, false);

    track.clear();

    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    // Should be silent
    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_FLOAT_EQ (rms, 0.0f);
}

} // namespace audio_plugin_test
