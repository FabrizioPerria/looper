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
        track.processRecord (input, samples);
        track.finalizeLayer();
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
    track.processRecord (input, maxBlockSize);
    track.finalizeLayer();

    int firstLength = track.getTrackLengthSamples();

    // Record overdub
    track.processRecord (input, maxBlockSize);
    track.finalizeLayer();

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

TEST (UndoIssueReproduction, ExactIssueScenario)
{
    LoopTrack track;
    const double sampleRate = 48000.0;
    const int maxBlockSize = 512;
    const int numChannels = 2;
    const int maxSeconds = 10;
    const int undoLayers = 5;

    track.prepareToPlay (sampleRate, maxBlockSize, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.toggleNormalizingOutput(); // Disable normalization for clearer testing

    // Step 1: Record first loop with amplitude 0.5
    std::cout << "\n=== Step 1: Recording first loop ===" << std::endl;
    juce::AudioBuffer<float> input1 (numChannels, maxBlockSize);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input1.getWritePointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (input1, maxBlockSize);
    track.finalizeLayer();

    // Verify first loop is recorded
    float firstLoopValue = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "First loop value at sample 10: " << firstLoopValue << std::endl;
    EXPECT_NEAR (firstLoopValue, 0.5f, 0.01f);

    // Step 2: Overdub with amplitude 0.3
    std::cout << "\n=== Step 2: Overdubbing ===" << std::endl;
    juce::AudioBuffer<float> input2 (numChannels, maxBlockSize);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input2.getWritePointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
            data[i] = 0.3f;
    }
    track.processRecord (input2, maxBlockSize);
    track.finalizeLayer();

    // Verify overdub combined the audio
    float overdubValue = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "After overdub value at sample 10: " << overdubValue << std::endl;
    EXPECT_NEAR (overdubValue, 0.8f, 0.01f); // 0.5 + 0.3 = 0.8

    // Step 3: First undo - should restore first loop (0.5)
    std::cout << "\n=== Step 3: First undo ===" << std::endl;
    bool undoSuccess1 = track.undo();
    EXPECT_TRUE (undoSuccess1);

    float afterFirstUndo = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "After first undo value at sample 10: " << afterFirstUndo << std::endl;

    // THE BUG: Without the fix, this would be ~0.0 (silence or near silence)
    // WITH the fix, this should be ~0.5 (first loop restored)
    EXPECT_NEAR (afterFirstUndo, 0.5f, 0.01f) << "FAILED: First undo should restore first loop, not silence!";

    // Step 4: Second undo - should do nothing (only one layer in undo stack)
    std::cout << "\n=== Step 4: Second undo (should do nothing) ===" << std::endl;
    bool undoSuccess2 = track.undo();

    float afterSecondUndo = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "After second undo value at sample 10: " << afterSecondUndo << std::endl;

    // Should still be the first loop value
    EXPECT_NEAR (afterSecondUndo, 0.5f, 0.01f);

    // Additional verification: Try to overdub again after undo
    std::cout << "\n=== Step 5: Overdub again after undo ===" << std::endl;
    juce::AudioBuffer<float> input3 (numChannels, maxBlockSize);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = input3.getWritePointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
            data[i] = 0.2f;
    }
    track.processRecord (input3, maxBlockSize);
    track.finalizeLayer();

    float afterSecondOverdub = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "After second overdub value at sample 10: " << afterSecondOverdub << std::endl;
    EXPECT_NEAR (afterSecondOverdub, 0.7f, 0.01f); // 0.5 + 0.2 = 0.7

    // Undo this second overdub
    std::cout << "\n=== Step 6: Undo second overdub ===" << std::endl;
    bool undoSuccess3 = track.undo();
    EXPECT_TRUE (undoSuccess3);

    float afterThirdUndo = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "After third undo value at sample 10: " << afterThirdUndo << std::endl;
    EXPECT_NEAR (afterThirdUndo, 0.5f, 0.01f) << "Should restore to first loop again";
}
// Ultimate diagnostic test - run this and send me the output!
// This will show us exactly what's in each buffer at each step

#include "engine/LoopTrack.h"
#include "engine/UndoManager.h"
#include <iomanip>
#include <iostream>

void dumpBuffer (const char* label, const juce::AudioBuffer<float>& buf, int samples = 20)
{
    std::cout << label << " [" << buf.getNumSamples() << " samples]: ";
    for (int i = 0; i < std::min (samples, buf.getNumSamples()); i++)
    {
        float val = buf.getSample (0, i);
        if (std::abs (val) < 0.0001f)
        {
            std::cout << "0 ";
        }
        else
        {
            std::cout << std::fixed << std::setprecision (2) << val << " ";
        }
    }
    std::cout << std::endl;
}

void dumpUndoStack (LoopTrack& track)
{
    // This is a hack - we can't directly access the undo manager,
    // but we can infer its state by trying operations
    std::cout << "  [Cannot directly inspect undo stack without exposing internals]" << std::endl;
}

// TEST (UndoIssueReproduction, ComprehensiveDiagnosticTest)
// {
//     std::cout << "\n============================================\n";
//     std::cout << "COMPREHENSIVE UNDO BUG DIAGNOSTIC TEST\n";
//     std::cout << "============================================\n\n";
//
//     LoopTrack track;
//     track.prepareToPlay (48000.0, 512, 2, 10, 5);
//     track.setCrossFadeLength (0);
//
//     // Check if normalization is on
//     bool normalizationOn = track.isOutputNormalized();
//     std::cout << "Output normalization: " << (normalizationOn ? "ON" : "OFF") << std::endl;
//     if (normalizationOn)
//     {
//         std::cout << "  Disabling normalization for clearer test..." << std::endl;
//         track.toggleNormalizingOutput();
//     }
//
//     std::cout << "\n--- STEP 1: Record first loop (all samples = 0.5) ---\n";
//     juce::AudioBuffer<float> input1 (2, 512);
//     input1.clear();
//     for (int i = 0; i < 512; i++)
//     {
//         input1.setSample (0, i, 0.5f);
//         input1.setSample (1, i, 0.5f);
//     }
//
//     std::cout << "Calling processRecord with 512 samples of 0.5..." << std::endl;
//     track.processRecord (input1, 512);
//     std::cout << "  isCurrentlyRecording: " << track.isCurrentlyRecording() << std::endl;
//
//     std::cout << "Calling finalizeLayer..." << std::endl;
//     track.finalizeLayer();
//     std::cout << "  isCurrentlyRecording: " << track.isCurrentlyRecording() << std::endl;
//     std::cout << "  Track length: " << track.getTrackLengthSamples() << " samples" << std::endl;
//
//     dumpBuffer ("  Buffer after first record", *track.getAudioBuffer());
//
//     std::cout << "\n--- STEP 2: Overdub (all samples = 0.3) ---\n";
//     juce::AudioBuffer<float> input2 (2, 512);
//     input2.clear();
//     for (int i = 0; i < 512; i++)
//     {
//         input2.setSample (0, i, 0.3f);
//         input2.setSample (1, i, 0.3f);
//     }
//
//     std::cout << "Calling processRecord for overdub..." << std::endl;
//     track.processRecord (input2, 512);
//     std::cout << "  isCurrentlyRecording: " << track.isCurrentlyRecording() << std::endl;
//     dumpBuffer ("  Buffer during overdub (before finalize)", *track.getAudioBuffer());
//
//     std::cout << "Calling finalizeLayer..." << std::endl;
//     track.finalizeLayer();
//     std::cout << "  isCurrentlyRecording: " << track.isCurrentlyRecording() << std::endl;
//     std::cout << "  Track length: " << track.getTrackLengthSamples() << " samples" << std::endl;
//
//     dumpBuffer ("  Buffer after overdub", *track.getAudioBuffer());
//     float overdubValue = track.getAudioBuffer()->getSample (0, 10);
//     std::cout << "  Sample[10] = " << overdubValue << " (expected ~0.8)" << std::endl;
//
//     std::cout << "\n--- STEP 3: First UNDO ---\n";
//     std::cout << "Calling undo()..." << std::endl;
//     bool undo1Success = track.undo();
//     std::cout << "  Undo returned: " << (undo1Success ? "TRUE" : "FALSE") << std::endl;
//     std::cout << "  Track length: " << track.getTrackLengthSamples() << " samples" << std::endl;
//
//     dumpBuffer ("  Buffer after first undo", *track.getAudioBuffer());
//     float undo1Value = track.getAudioBuffer()->getSample (0, 10);
//     std::cout << "  Sample[10] = " << undo1Value << " (expected ~0.5)" << std::endl;
//
//     // Check if it's silence
//     bool isSilence = true;
//     for (int i = 0; i < std::min (100, track.getAudioBuffer()->getNumSamples()); i++)
//     {
//         if (std::abs (track.getAudioBuffer()->getSample (0, i)) > 0.0001f)
//         {
//             isSilence = false;
//             break;
//         }
//     }
//     EXPECT_FALSE (isSilence) << "Buffer is SILENT after first undo!";
//     EXPECT_NEAR (undo1Value, 0.5f, 0.1f) << "Wrong value after undo (expected ~0.5)";
//
//     if (isSilence)
//     {
//         std::cout << "\n  *** BUG DETECTED: Buffer is SILENT after first undo! ***\n" << std::endl;
//     }
//     else if (std::abs (undo1Value - 0.5f) > 0.1f)
//     {
//         std::cout << "\n  *** BUG DETECTED: Wrong value after undo (expected 0.5, got " << undo1Value << ") ***\n" << std::endl;
//     }
//     else
//     {
//         std::cout << "\n  OK: First undo seems correct\n" << std::endl;
//     }
//
//     std::cout << "\n--- STEP 4: Second UNDO ---\n";
//     std::cout << "Calling undo() again..." << std::endl;
//     bool undo2Success = track.undo();
//     std::cout << "  Undo returned: " << (undo2Success ? "TRUE" : "FALSE") << std::endl;
//     std::cout << "  Track length: " << track.getTrackLengthSamples() << " samples" << std::endl;
//
//     dumpBuffer ("  Buffer after second undo", *track.getAudioBuffer());
//     float undo2Value = track.getAudioBuffer()->getSample (0, 10);
//     std::cout << "  Sample[10] = " << undo2Value << std::endl;
//
//     if (undo2Success && std::abs (undo2Value - 0.5f) < 0.1f && isSilence)
//     {
//         std::cout << "\n  *** This matches the bug description! ***" << std::endl;
//         std::cout << "  First undo gave silence, second undo gave the first loop." << std::endl;
//     }
//
//     std::cout << "\n--- ADDITIONAL TEST: Overdub after undo ---\n";
//     track.clear();
//
//     // Repeat the scenario
//     track.processRecord (input1, 512);
//     track.finalizeLayer();
//
//     track.processRecord (input2, 512);
//     track.finalizeLayer();
//
//     track.undo();
//
//     std::cout << "After undo, trying to overdub again with 0.2..." << std::endl;
//     juce::AudioBuffer<float> input3 (2, 512);
//     input3.clear();
//     for (int i = 0; i < 512; i++)
//     {
//         input3.setSample (0, i, 0.2f);
//         input3.setSample (1, i, 0.2f);
//     }
//
//     track.processRecord (input3, 512);
//     track.finalizeLayer();
//
//     dumpBuffer ("  Buffer after new overdub", *track.getAudioBuffer());
//     float newOverdubValue = track.getAudioBuffer()->getSample (0, 10);
//     std::cout << "  Sample[10] = " << newOverdubValue << " (expected ~0.7 = 0.5 + 0.2)" << std::endl;
//
//     std::cout << "\nNow undo this new overdub..." << std::endl;
//     track.undo();
//     dumpBuffer ("  Buffer after undoing new overdub", *track.getAudioBuffer());
//     float finalValue = track.getAudioBuffer()->getSample (0, 10);
//     std::cout << "  Sample[10] = " << finalValue << " (expected ~0.5)" << std::endl;
//
//     EXPECT_NEAR (finalValue, 0.5f, 0.1f) << "Final undo should restore to first loop value (~0.5)";
//
//     if (std::abs (finalValue - 0.5f) > 0.1f)
//     {
//         std::cout << "\n  *** BUG: Wrong buffer restored! This proves the staging buffer issue! ***\n" << std::endl;
//     }
// }

// Additional test: Multiple overdub/undo cycles
TEST (UndoIssueReproduction, MultipleOverdubUndoCycles)
{
    LoopTrack track;
    const double sampleRate = 48000.0;
    const int maxBlockSize = 512;
    const int numChannels = 2;
    const int maxSeconds = 10;
    const int undoLayers = 5;

    track.prepareToPlay (sampleRate, maxBlockSize, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.toggleNormalizingOutput();

    // Record base loop
    juce::AudioBuffer<float> baseInput (numChannels, maxBlockSize);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = baseInput.getWritePointer (ch);
        for (int i = 0; i < maxBlockSize; ++i)
            data[i] = 0.5f;
    }
    track.processRecord (baseInput, maxBlockSize);
    track.finalizeLayer();

    float baseValue = track.getAudioBuffer()->getSample (0, 10);
    std::cout << "Base value: " << baseValue << std::endl;

    // Perform multiple overdub/undo cycles
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        std::cout << "\n=== Cycle " << (cycle + 1) << " ===" << std::endl;

        // Overdub
        float overdubAmount = 0.1f * (cycle + 1);
        juce::AudioBuffer<float> overdubInput (numChannels, maxBlockSize);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = overdubInput.getWritePointer (ch);
            for (int i = 0; i < maxBlockSize; ++i)
                data[i] = overdubAmount;
        }
        track.processRecord (overdubInput, maxBlockSize);
        track.finalizeLayer();

        float afterOverdub = track.getAudioBuffer()->getSample (0, 10);
        std::cout << "  After overdub: " << afterOverdub << std::endl;
        EXPECT_NEAR (afterOverdub, baseValue + overdubAmount, 0.01f);

        // Undo
        track.undo();
        float afterUndo = track.getAudioBuffer()->getSample (0, 10);
        std::cout << "  After undo: " << afterUndo << std::endl;
        EXPECT_NEAR (afterUndo, baseValue, 0.01f) << "Cycle " << cycle << ": Undo should restore base value";
    }
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
    track.processRecord (input1, maxBlockSize);
    track.finalizeLayer();

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
    track.processRecord (input2, maxBlockSize);
    track.finalizeLayer();

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
    track.processRecord (input, maxBlockSize);
    track.finalizeLayer();

    track.processRecord (input, maxBlockSize);
    track.finalizeLayer();

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
    track.processPlayback (output1, maxBlockSize);
    float rms1 = output1.getRMSLevel (0, 0, maxBlockSize);

    // Undo and redo
    track.undo();
    track.redo();

    // Verify audio matches
    juce::AudioBuffer<float> output2 (numChannels, maxBlockSize);
    output2.clear();
    track.processPlayback (output2, maxBlockSize);
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
    track.processPlayback (output, maxBlockSize);

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
    track.processPlayback (output, maxBlockSize);

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
        track.processPlayback (output, maxBlockSize);
    }

    // Undo should still work
    track.undo();
    EXPECT_EQ (track.getTrackLengthSamples(), 1000);
}

} // namespace audio_plugin_test
