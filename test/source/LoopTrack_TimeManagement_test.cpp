#include "engine/LoopTrack.h"
#include <gtest/gtest.h>

namespace audio_plugin_test
{

class LoopTrackTimeManagementTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        track.prepareToPlay (sampleRate, maxBlockSize, numChannels, maxSeconds, undoLayers);
        track.setCrossFadeLength (0);
    }

    void recordTestLoop (int samples, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> input (numChannels, samples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = input.getWritePointer (ch);
            for (int i = 0; i < samples; ++i)
                data[i] = amplitude;
        }
        track.processRecord (input, samples);
        track.finalizeLayer();
    }

    LoopTrack track;
    const double sampleRate = 48000.0;
    const int maxBlockSize = 512;
    const int numChannels = 2;
    const int maxSeconds = 10;
    const int undoLayers = 1;
};

// ============================================================================
// Playback Speed Tests
// ============================================================================

TEST_F (LoopTrackTimeManagementTest, SetAndGetPlaybackSpeed)
{
    track.setPlaybackSpeed (0.5f);
    EXPECT_FLOAT_EQ (track.getPlaybackSpeed(), 0.5f);

    track.setPlaybackSpeed (2.0f);
    EXPECT_FLOAT_EQ (track.getPlaybackSpeed(), 2.0f);

    track.setPlaybackSpeed (1.0f);
    EXPECT_FLOAT_EQ (track.getPlaybackSpeed(), 1.0f);
}

TEST_F (LoopTrackTimeManagementTest, SlowPlaybackWorks)
{
    recordTestLoop (10000, 0.5f);

    // Play at half speed
    track.setPlaybackSpeed (0.5f);
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize);

    // Should have audio
    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackTimeManagementTest, FastPlaybackWorks)
{
    recordTestLoop (10000, 0.5f);

    // Play at double speed
    track.setPlaybackSpeed (2.0f);
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize);

    // Should have audio
    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackTimeManagementTest, DJSlowdown)
{
    recordTestLoop (48000, 0.5f); // 1 second loop

    // Simulate DJ slowdown from 1.0x to 0.5x
    float speeds[] = { 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f };

    for (float speed : speeds)
    {
        track.setPlaybackSpeed (speed);
        juce::AudioBuffer<float> output (numChannels, maxBlockSize);
        output.clear();
        track.processPlayback (output, maxBlockSize);

        // Should produce audio at all speeds
        float rms = output.getRMSLevel (0, 0, maxBlockSize);
        EXPECT_GT (rms, 0.0f);
    }
}

TEST_F (LoopTrackTimeManagementTest, ExtendedPlaybackAllSpeeds)
{
    recordTestLoop (10000, 0.5f);

    // Test extended playback at different speeds
    float testSpeeds[] = { 0.5f, 1.0f, 1.5f, 2.0f };

    for (float speed : testSpeeds)
    {
        track.setPlaybackSpeed (speed);

        // Play for equivalent of 10 loop cycles
        int blocksToPlay = (int) ((10000 * 10) / maxBlockSize / speed) + 1;

        for (int i = 0; i < blocksToPlay; ++i)
        {
            juce::AudioBuffer<float> output (numChannels, maxBlockSize);
            output.clear();
            track.processPlayback (output, maxBlockSize);

            // Should never produce silence or invalid position
            float rms = output.getRMSLevel (0, 0, maxBlockSize);
            EXPECT_GT (rms, 0.0f);

            int pos = track.getCurrentReadPosition();
            EXPECT_LT (pos, track.getTrackLengthSamples());
        }
    }
}

TEST_F (LoopTrackTimeManagementTest, RapidSpeedChanges)
{
    recordTestLoop (48000, 0.5f);

    // Rapidly change speeds
    float speeds[] = { 2.0f, 0.5f, 1.5f, 0.3f, 1.8f, 0.7f, 1.0f };

    for (int iteration = 0; iteration < 5; ++iteration)
    {
        for (float speed : speeds)
        {
            track.setPlaybackSpeed (speed);
            juce::AudioBuffer<float> output (numChannels, maxBlockSize);
            output.clear();
            track.processPlayback (output, maxBlockSize);

            // Should remain stable
            float rms = output.getRMSLevel (0, 0, maxBlockSize);
            EXPECT_GT (rms, 0.0f);

            int pos = track.getCurrentReadPosition();
            EXPECT_LT (pos, track.getTrackLengthSamples());
        }
    }
}

// ============================================================================
// Playback Direction Tests
// ============================================================================

TEST_F (LoopTrackTimeManagementTest, SetAndGetPlaybackDirection)
{
    EXPECT_TRUE (track.isPlaybackDirectionForward());

    track.setPlaybackDirectionBackward();
    EXPECT_FALSE (track.isPlaybackDirectionForward());

    track.setPlaybackDirectionForward();
    EXPECT_TRUE (track.isPlaybackDirectionForward());
}

TEST_F (LoopTrackTimeManagementTest, ReversePlaybackWorks)
{
    recordTestLoop (10000, 0.5f);

    // Play in reverse
    track.setPlaybackDirectionBackward();
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize);

    // Should have audio
    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackTimeManagementTest, BackspinEffect)
{
    recordTestLoop (48000, 0.5f); // 1 second loop

    // Move forward a bit
    juce::AudioBuffer<float> dummy (numChannels, 10000);
    track.processPlayback (dummy, 10000);
    int forwardPos = track.getCurrentReadPosition();

    // Quick reverse (backspin effect)
    track.setPlaybackSpeed (2.0f);
    track.setPlaybackDirectionBackward();

    // Play several blocks going backward
    for (int i = 0; i < 10; ++i)
    {
        juce::AudioBuffer<float> output (numChannels, maxBlockSize);
        output.clear();
        track.processPlayback (output, maxBlockSize);
    }

    int backspinPos = track.getCurrentReadPosition();

    // Position should have changed from going backward
    EXPECT_NE (backspinPos, forwardPos);

    // Return to forward at normal speed
    track.setPlaybackSpeed (1.0f);
    track.setPlaybackDirectionForward();

    // Let it stabilize and verify we can still play
    for (int i = 0; i < 5; ++i)
    {
        juce::AudioBuffer<float> output (numChannels, maxBlockSize);
        output.clear();
        track.processPlayback (output, maxBlockSize);
    }

    // Just verify the system is still stable (doesn't crash)
    EXPECT_GT (track.getTrackLengthSamples(), 0);
}

TEST_F (LoopTrackTimeManagementTest, RapidDirectionChanges)
{
    recordTestLoop (48000, 0.5f);

    // Move to middle
    juce::AudioBuffer<float> dummy (numChannels, 24000);
    track.processPlayback (dummy, 24000);

    int trackLength = track.getTrackLengthSamples();

    // Rapidly toggle direction - mainly testing that it doesn't crash
    for (int i = 0; i < 50; ++i)
    {
        if (i % 2 == 0)
            track.setPlaybackDirectionForward();
        else
            track.setPlaybackDirectionBackward();

        juce::AudioBuffer<float> output (numChannels, maxBlockSize);
        output.clear();
        track.processPlayback (output, maxBlockSize);

        // Position should remain valid
        int pos = track.getCurrentReadPosition();
        EXPECT_GE (pos, 0);
        EXPECT_LT (pos, trackLength);
    }

    // After all the changes, should eventually produce audio again
    track.setPlaybackDirectionForward();
    track.setPlaybackSpeed (1.0f);

    bool hasAudio = false;
    for (int i = 0; i < 10; ++i)
    {
        juce::AudioBuffer<float> output (numChannels, maxBlockSize);
        output.clear();
        track.processPlayback (output, maxBlockSize);

        float rms = output.getRMSLevel (0, 0, maxBlockSize);
        if (rms > 0.0f)
        {
            hasAudio = true;
            break;
        }
    }

    EXPECT_TRUE (hasAudio);
}

TEST_F (LoopTrackTimeManagementTest, ExtremeSpeedAndDirection)
{
    recordTestLoop (48000, 0.5f);

    // Test extreme combinations
    struct TestCase
    {
        float speed;
        bool forward;
    };

    TestCase testCases[] = {
        { 0.2f, true },  // Slowest forward
        { 0.2f, false }, // Slowest reverse
        { 2.0f, true },  // Fastest forward
        { 2.0f, false }, // Fastest reverse
    };

    for (const auto& testCase : testCases)
    {
        track.setPlaybackSpeed (testCase.speed);
        if (testCase.forward)
            track.setPlaybackDirectionForward();
        else
            track.setPlaybackDirectionBackward();

        // Play for several blocks
        for (int i = 0; i < 20; ++i)
        {
            juce::AudioBuffer<float> output (numChannels, maxBlockSize);
            output.clear();
            track.processPlayback (output, maxBlockSize);

            // Should remain stable even at extremes
            float rms = output.getRMSLevel (0, 0, maxBlockSize);
            EXPECT_GT (rms, 0.0f);

            int pos = track.getCurrentReadPosition();
            EXPECT_LT (pos, track.getTrackLengthSamples());
        }
    }
}

// ============================================================================
// Pitch Preservation Tests
// ============================================================================

TEST_F (LoopTrackTimeManagementTest, KeepPitchWhenChangingSpeed)
{
    EXPECT_FALSE (track.shouldKeepPitchWhenChangingSpeed());

    track.setKeepPitchWhenChangingSpeed (true);
    EXPECT_TRUE (track.shouldKeepPitchWhenChangingSpeed());

    track.setKeepPitchWhenChangingSpeed (false);
    EXPECT_FALSE (track.shouldKeepPitchWhenChangingSpeed());
}

TEST_F (LoopTrackTimeManagementTest, KeepPitchDoesNotCrashDuringPlayback)
{
    recordTestLoop (10000, 0.5f);

    // Enable pitch preservation
    track.setKeepPitchWhenChangingSpeed (true);
    track.setPlaybackSpeed (1.5f);

    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize);

    // Should produce audio without crashing
    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackTimeManagementTest, PitchModeChangeDuringPlayback)
{
    recordTestLoop (10000, 0.5f);

    track.setPlaybackSpeed (1.5f);

    // Play with pitch changes
    track.setKeepPitchWhenChangingSpeed (false);
    juce::AudioBuffer<float> output1 (numChannels, maxBlockSize);
    output1.clear();
    track.processPlayback (output1, maxBlockSize);
    EXPECT_GT (output1.getRMSLevel (0, 0, maxBlockSize), 0.0f);

    // Switch to pitch preservation mid-playback
    track.setKeepPitchWhenChangingSpeed (true);
    juce::AudioBuffer<float> output2 (numChannels, maxBlockSize);
    output2.clear();
    track.processPlayback (output2, maxBlockSize);
    EXPECT_GT (output2.getRMSLevel (0, 0, maxBlockSize), 0.0f);

    // Switch back
    track.setKeepPitchWhenChangingSpeed (false);
    juce::AudioBuffer<float> output3 (numChannels, maxBlockSize);
    output3.clear();
    track.processPlayback (output3, maxBlockSize);
    EXPECT_GT (output3.getRMSLevel (0, 0, maxBlockSize), 0.0f);
}

} // namespace audio_plugin_test
