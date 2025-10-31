#include "engine/LoopTrack.h"
#include <gtest/gtest.h>

namespace audio_plugin_test
{

class LoopTrackStateManagementTest : public ::testing::Test
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
        track.processRecord (input, samples, false);
        track.finalizeLayer (false);
    }

    LoopTrack track;
    const double sampleRate = 48000.0;
    const int maxBlockSize = 512;
    const int numChannels = 2;
    const int maxSeconds = 10;
    const int undoLayers = 3; // Multiple undo layers for testing
};

// ============================================================================
// Undo Tests
// ============================================================================

TEST_F (LoopTrackStateManagementTest, UndoRestoresPreviousState)
{
    // Record first layer
    juce::AudioBuffer<float> input (numChannels, maxBlockSize);
    input.clear();
    track.processRecord (input, maxBlockSize, false);
    track.finalizeLayer (false);

    int firstLength = track.getTrackLengthSamples();

    // Record overdub
    track.processRecord (input, maxBlockSize, true);
    track.finalizeLayer (true);

    // Undo should restore first length
    track.undo();

    EXPECT_EQ (track.getTrackLengthSamples(), firstLength);
}

TEST_F (LoopTrackStateManagementTest, UndoOnEmptyTrackDoesNothing)
{
    track.undo();

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
    EXPECT_EQ (track.getCurrentReadPosition(), 0);
}

TEST_F (LoopTrackStateManagementTest, MultipleUndoLevels)
{
    // Record three layers
    recordTestLoop (1000, 0.3f);
    int length1 = track.getTrackLengthSamples();

    recordTestLoop (1000, 0.4f); // Overdub
    recordTestLoop (1000, 0.5f); // Overdub

    // Undo twice
    track.undo();
    track.undo();

    // Should be back to first layer
    EXPECT_EQ (track.getTrackLengthSamples(), length1);
}

TEST_F (LoopTrackStateManagementTest, UndoActualBehaviorTest)
{
    // Let's test what ACTUALLY happens with your implementation

    // Record first layer
    recordTestLoop (maxBlockSize, 0.5f);

    // At this point, staging buffer should have the normalized first layer
    // Let's verify by doing an overdub

    float* firstLayerPtr = track.getAudioBuffer()->getWritePointer (0);
    std::vector<float> firstLayerSamples (firstLayerPtr, firstLayerPtr + 10);

    std::cout << "First layer samples: ";
    for (int i = 0; i < 10; i++)
        std::cout << firstLayerSamples[i] << " ";
    std::cout << std::endl;

    // Overdub
    recordTestLoop (maxBlockSize, 0.3f);

    std::cout << "After overdub samples: ";
    for (int i = 0; i < 10; i++)
        std::cout << track.getAudioBuffer()->getSample (0, i) << " ";
    std::cout << std::endl;

    // Undo
    track.undo();

    std::cout << "After undo samples: ";
    for (int i = 0; i < 10; i++)
        std::cout << track.getAudioBuffer()->getSample (0, i) << " ";
    std::cout << std::endl;

    // What do we expect? We should see the first layer values restored
}

TEST_F (LoopTrackStateManagementTest, UndoRestoresNormalizedFirstLayer)
{
    // Record first layer - input 0.5, will be normalized
    juce::AudioBuffer<float> input1 (numChannels, maxBlockSize);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input1.getWritePointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (input1, maxBlockSize, false);
    track.finalizeLayer (false);

    // After normalization, it should be 0.9 (0.5 * (0.9/0.5))
    float firstLayerNormalized = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "First layer after normalize: " << firstLayerNormalized << std::endl;

    // Overdub - input 0.3, will ADD to existing
    juce::AudioBuffer<float> input2 (numChannels, maxBlockSize);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input2.getWritePointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
            data[i] = 0.3f;
    }
    track.processRecord (input2, maxBlockSize, true);
    track.finalizeLayer (true);

    float afterOverdub = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "After overdub: " << afterOverdub << std::endl;

    // Undo should restore the NORMALIZED first layer value
    track.undo();

    float afterUndo = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "After undo: " << afterUndo << std::endl;

    EXPECT_NEAR (firstLayerNormalized, afterUndo, 0.01f);
}

TEST_F (LoopTrackStateManagementTest, UndoAfterRecordingOnlyDoesNothing)
{
    recordTestLoop (maxBlockSize);

    // Undo on a track with only one layer should do nothing
    int lengthBefore = track.getTrackLengthSamples();
    track.undo();
    int lengthAfter = track.getTrackLengthSamples();

    EXPECT_EQ (lengthBefore, lengthAfter);
}

// ============================================================================
// Redo Tests
// ============================================================================

TEST_F (LoopTrackStateManagementTest, RedoRestoresUndoneState)
{
    // Record two layers
    juce::AudioBuffer<float> input (numChannels, maxBlockSize);
    input.clear();
    track.processRecord (input, maxBlockSize, false);
    track.finalizeLayer (false);

    track.processRecord (input, maxBlockSize, true);
    track.finalizeLayer (true);

    int secondLength = track.getTrackLengthSamples();

    // Undo then redo
    track.undo();
    track.redo();

    EXPECT_EQ (track.getTrackLengthSamples(), secondLength);
}

TEST_F (LoopTrackStateManagementTest, RedoOnEmptyStackDoesNothing)
{
    recordTestLoop (maxBlockSize);

    // Redo without undo does nothing
    int lengthBefore = track.getTrackLengthSamples();
    track.redo();
    int lengthAfter = track.getTrackLengthSamples();

    EXPECT_EQ (lengthBefore, lengthAfter);
}

TEST_F (LoopTrackStateManagementTest, MultipleRedoLevels)
{
    // Record three layers
    recordTestLoop (1000, 0.3f);
    recordTestLoop (1000, 0.4f); // Overdub
    recordTestLoop (1000, 0.5f); // Overdub

    int finalLength = track.getTrackLengthSamples();

    // Undo twice
    track.undo();
    track.undo();

    // Redo twice
    track.redo();
    track.redo();

    // Should be back to final state
    EXPECT_EQ (track.getTrackLengthSamples(), finalLength);
}

TEST_F (LoopTrackStateManagementTest, RedoPreservesAudioContent)
{
    // Record and overdub
    recordTestLoop (maxBlockSize, 0.3f);
    recordTestLoop (maxBlockSize, 0.5f); // Overdub

    // Get RMS after overdub
    juce::AudioBuffer<float> output1 (numChannels, maxBlockSize);
    output1.clear();
    track.processPlayback (output1, maxBlockSize, false);
    float rms1 = output1.getRMSLevel (0, 0, maxBlockSize);

    // Undo and redo
    track.undo();
    track.redo();

    // Verify audio matches
    juce::AudioBuffer<float> output2 (numChannels, maxBlockSize);
    output2.clear();
    track.processPlayback (output2, maxBlockSize, false);
    float rms2 = output2.getRMSLevel (0, 0, maxBlockSize);

    EXPECT_NEAR (rms1, rms2, 0.01f);
}

TEST_F (LoopTrackStateManagementTest, NewRecordingClearsRedoStack)
{
    // Record, overdub, undo
    recordTestLoop (1000, 0.3f);
    recordTestLoop (1000, 0.4f); // Overdub
    track.undo();

    // New recording should clear redo stack
    recordTestLoop (1000, 0.5f); // New overdub

    // Redo should do nothing now
    int lengthBefore = track.getTrackLengthSamples();
    track.redo();
    int lengthAfter = track.getTrackLengthSamples();

    EXPECT_EQ (lengthBefore, lengthAfter);
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST_F (LoopTrackStateManagementTest, ClearResetsAllState)
{
    // Record something
    recordTestLoop (maxBlockSize);

    EXPECT_GT (track.getTrackLengthSamples(), 0);

    track.clear();

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
    EXPECT_EQ (track.getCurrentReadPosition(), 0);
    EXPECT_EQ (track.getCurrentWritePosition(), 0);
}

TEST_F (LoopTrackStateManagementTest, ClearMakesBufferSilent)
{
    recordTestLoop (maxBlockSize, 0.5f);

    track.clear();

    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    // Should be silent
    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_FLOAT_EQ (rms, 0.0f);
}

TEST_F (LoopTrackStateManagementTest, ClearClearsUndoStack)
{
    // Record multiple layers
    recordTestLoop (1000, 0.3f);
    recordTestLoop (1000, 0.4f);

    track.clear();

    // Undo should do nothing
    track.undo();
    EXPECT_EQ (track.getTrackLengthSamples(), 0);
}

TEST_F (LoopTrackStateManagementTest, ClearAllowsNewRecording)
{
    recordTestLoop (maxBlockSize);
    track.clear();

    // Should be able to record fresh
    recordTestLoop (maxBlockSize, 0.5f);

    EXPECT_GT (track.getTrackLengthSamples(), 0);

    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    output.clear();
    track.processPlayback (output, maxBlockSize, false);

    float rms = output.getRMSLevel (0, 0, maxBlockSize);
    EXPECT_GT (rms, 0.0f);
}

TEST_F (LoopTrackStateManagementTest, ClearOnEmptyTrackDoesNothing)
{
    track.clear();

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
    EXPECT_EQ (track.getCurrentReadPosition(), 0);
}

// ============================================================================
// Combined State Management Tests
// ============================================================================

TEST_F (LoopTrackStateManagementTest, UndoRedoUndoSequence)
{
    recordTestLoop (1000, 0.3f);
    int length1 = track.getTrackLengthSamples();

    recordTestLoop (1000, 0.4f);
    int length2 = track.getTrackLengthSamples();

    track.undo();
    EXPECT_EQ (track.getTrackLengthSamples(), length1);

    track.redo();
    EXPECT_EQ (track.getTrackLengthSamples(), length2);

    track.undo();
    EXPECT_EQ (track.getTrackLengthSamples(), length1);
}

TEST_F (LoopTrackStateManagementTest, MaxUndoLayersRespected)
{
    // Record more than max undo layers
    for (int i = 0; i < undoLayers + 2; ++i)
    {
        recordTestLoop (1000, 0.3f + i * 0.1f);
    }

    // Undo max times
    for (int i = 0; i < undoLayers; ++i)
    {
        track.undo();
    }

    // Should still have content (oldest layer)
    EXPECT_GT (track.getTrackLengthSamples(), 0);
}

TEST_F (LoopTrackStateManagementTest, StatePreservedAcrossPlayback)
{
    recordTestLoop (1000, 0.5f);
    recordTestLoop (1000, 0.6f);

    // Play back
    juce::AudioBuffer<float> output (numChannels, maxBlockSize);
    for (int i = 0; i < 10; ++i)
    {
        output.clear();
        track.processPlayback (output, maxBlockSize, false);
    }

    // Undo should still work
    track.undo();
    EXPECT_EQ (track.getTrackLengthSamples(), 1000);
}

} // namespace audio_plugin_test
