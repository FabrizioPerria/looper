#include "engine/LoopTrack.h"
#include <gtest/gtest.h>

namespace audio_plugin_test
{

// Helper function
juce::AudioBuffer<float> createAnotherSquareTestBuffer (int numChannels, int numSamples, double sr, float frequency)
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
// Recording Tests
// ============================================================================

class LoopTrackRecordingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        track.prepareToPlay (sampleRate, maxBlockSize, numChannels, maxSeconds, undoLayers);
        track.setCrossFadeLength (0); // Disable crossfade for cleaner testing
    }

    LoopTrack track;
    const double sampleRate = 48000.0;
    const int maxBlockSize = 512;
    const int numChannels = 2;
    const int maxSeconds = 10;
    const int undoLayers = 1;
};

TEST_F (LoopTrackRecordingTest, ProcessFullBlockCopiesInput)
{
    const int numSamples = 4;
    const double testSampleRate = 10.0;
    const int testBlockSize = 4;
    const int testChannels = 1;

    LoopTrack testTrack;
    testTrack.prepareToPlay (testSampleRate, testBlockSize, testChannels, 10, 1);
    testTrack.setCrossFadeLength (0);
    testTrack.setOverdubGainNew (1.0f);
    testTrack.setOverdubGainOld (1.0f);
    testTrack.toggleNormalizingOutput(); // Disable normalization for cleaner testing

    juce::AudioBuffer<float> input = createAnotherSquareTestBuffer (testChannels, numSamples, testSampleRate, 440.0f);
    auto* readPtr = input.getReadPointer (0);

    testTrack.processRecord (input, numSamples, false);

    const auto* loopBuffer = testTrack.getAudioBuffer();
    auto* loopPtr = loopBuffer->getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }

    EXPECT_EQ (testTrack.getTrackLengthSamples(), 0); // Not finalized yet

    // Process another block and check it appends correctly
    testTrack.processRecord (input, numSamples, false);
    testTrack.finalizeLayer (false);
    loopPtr = loopBuffer->getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i + numSamples], readPtr[i]);
    }

    EXPECT_EQ (testTrack.getTrackLengthSamples(), numSamples * 2);
}

TEST_F (LoopTrackRecordingTest, ProcessPartialBlockCopiesInput)
{
    const int numSamples = 9; // less than block size
    juce::AudioBuffer<float> input = createAnotherSquareTestBuffer (numChannels, numSamples, sampleRate, 440.0f);
    auto* readPtr = input.getReadPointer (0);

    track.processRecord (input, numSamples, false);

    const auto* loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer->getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }
}

TEST_F (LoopTrackRecordingTest, RecordingMultipleBlocks)
{
    const int numBlocks = 10;
    juce::AudioBuffer<float> input (numChannels, maxBlockSize);

    for (int block = 0; block < numBlocks; ++block)
    {
        input.clear();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = input.getWritePointer (ch);
            for (int i = 0; i < maxBlockSize; ++i)
            {
                data[i] = (float) block * 0.1f; // Different value per block
            }
        }

        track.processRecord (input, maxBlockSize, false);
    }

    track.finalizeLayer (false);

    EXPECT_EQ (track.getTrackLengthSamples(), numBlocks * maxBlockSize);
}

// TEST_F (LoopTrackRecordingTest, IsCurrentlyRecording)
// {
//     EXPECT_FALSE (track.isCurrentlyRecording());
//
//     juce::AudioBuffer<float> input (numChannels, maxBlockSize);
//     input.clear();
//     track.processRecord (input, maxBlockSize);
//
//     EXPECT_TRUE (track.isCurrentlyRecording());
//
//     track.finalizeLayer();
//
//     EXPECT_FALSE (track.isCurrentlyRecording());
// }

// TEST_F (LoopTrackRecordingTest, ZeroSamplesDoesNothing)
// {
//     juce::AudioBuffer<float> input (numChannels, maxBlockSize);
//     track.processRecord (input, 0);
//
//     EXPECT_FALSE (track.isCurrentlyRecording());
//     EXPECT_EQ (track.getCurrentWritePosition(), 0);
// }
//
// TEST_F (LoopTrackRecordingTest, InvalidBufferDoesNothing)
// {
//     // Wrong channel count
//     juce::AudioBuffer<float> wrongChannels (1, maxBlockSize);
//     track.processRecord (wrongChannels, maxBlockSize);
//
//     EXPECT_FALSE (track.isCurrentlyRecording());
// }

TEST_F (LoopTrackRecordingTest, WritePositionAdvances)
{
    int pos1 = track.getCurrentWritePosition();

    juce::AudioBuffer<float> input (numChannels, maxBlockSize);
    input.clear();
    track.processRecord (input, maxBlockSize, false);

    int pos2 = track.getCurrentWritePosition();

    EXPECT_NE (pos1, pos2);
}

TEST_F (LoopTrackRecordingTest, FinalizeLayerSetsLength)
{
    juce::AudioBuffer<float> input (numChannels, maxBlockSize);
    input.clear();

    track.processRecord (input, maxBlockSize, false);
    EXPECT_EQ (track.getTrackLengthSamples(), 0); // Not finalized

    track.finalizeLayer (false);
    EXPECT_GT (track.getTrackLengthSamples(), 0); // Now finalized
}

// ============================================================================
// Overdub Recording Tests
// ============================================================================

TEST_F (LoopTrackRecordingTest, OverdubAddsToExistingLayer)
{
    // Record initial layer
    juce::AudioBuffer<float> input1 (numChannels, maxBlockSize);
    input1.clear();
    float* data1 = input1.getWritePointer (0);
    for (int i = 0; i < maxBlockSize; ++i)
        data1[i] = 0.3f;

    track.processRecord (input1, maxBlockSize, false);
    track.finalizeLayer (false);

    int loopLength = track.getTrackLengthSamples();

    // Overdub new layer
    juce::AudioBuffer<float> input2 (numChannels, maxBlockSize);
    input2.clear();
    float* data2 = input2.getWritePointer (0);
    for (int i = 0; i < maxBlockSize; ++i)
        data2[i] = 0.2f;

    track.processRecord (input2, maxBlockSize, true);
    track.finalizeLayer (true);

    // Length should not change during overdub
    EXPECT_EQ (track.getTrackLengthSamples(), loopLength);

    // Play back and verify it's louder (combined)
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.25f); // Should be higher than either input alone
}

TEST_F (LoopTrackRecordingTest, OverdubDoesNotChangeLengthDuringCycle)
{
    // Record initial layer
    juce::AudioBuffer<float> input (numChannels, 1000);
    input.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 1000; ++i)
            data[i] = 0.5f;
    }

    track.processRecord (input, 1000, false);
    track.finalizeLayer (false);

    int originalLength = track.getTrackLengthSamples();

    // Start overdubbing - record exactly the same length
    track.processRecord (input, 1000, true);
    track.finalizeLayer (true);

    EXPECT_EQ (track.getTrackLengthSamples(), originalLength);
}

TEST_F (LoopTrackRecordingTest, OverdubStopsAtLoopEnd)
{
    // Record short initial layer
    juce::AudioBuffer<float> input (numChannels, 500);
    input.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input.getWritePointer (ch);
        for (int i = 0; i < 500; ++i)
            data[i] = 0.5f;
    }

    track.processRecord (input, 500, false);
    track.finalizeLayer (false);

    int originalLength = track.getTrackLengthSamples();

    // Try to overdub MORE than the loop length - should stop at loop boundary
    juce::AudioBuffer<float> longInput (numChannels, 1000);
    longInput.clear();

    track.processRecord (longInput, 1000, true);

    // Should have wrapped or stopped at the original length boundary
    EXPECT_LE (track.getCurrentWritePosition(), originalLength);
}

} // namespace audio_plugin_test
