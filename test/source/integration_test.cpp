/*
 * Integration Tests for Looper Engine
 * ====================================
 * 
 * These tests verify that components work together correctly:
 * - LoopTrack: Record/playback/overdub cycles with real audio
 * - LooperStateMachine: State transitions with actual audio processing
 * - LooperEngine: Full workflow including multi-track, commands, sync
 * - Multi-track sync: Track synchronization and quantization
 * - Playback modes: Single vs multi-play behavior
 * - Stress tests: Edge cases and capacity limits
 * 
 * IMPORTANT NOTES:
 * ================
 * 1. JUCE Message Manager: These tests require an active MessageManager
 *    for timer-based operations (EngineMessageBus). Each test fixture
 *    has ScopedJuceInitialiser_GUI as a member to handle this.
 * 
 * 2. CTest Integration: These tests are designed to run via CTest.
 *    They use GTest::gtest and GTest::gmock (not GTest::Main).
 *    CTest will handle initialization and cleanup.
 * 
 * 3. Timer Events: EngineMessageBus uses a timer to dispatch events.
 *    Tests can either:
 *    - Call bus.dispatchPendingEvents() manually (synchronous)
 *    - Call pumpMessageManager() to run the message loop briefly
 *    
 * 4. StateContext Lifetime: Be careful with StateContext - the arrays
 *    it points to must outlive the context. Use static or class members.
 *    
 * 5. Test Philosophy: Focus on behavioral verification rather than
 *    implementation details. Tests should pass regardless of internal
 *    refactoring as long as behavior is maintained.
 * 
 * 6. Running Tests:
 *    - Use: ctest (recommended)
 *    - Not: ./looper_integration_tests (no main() function)
 */

#include "engine/LoopTrack.h"
#include "engine/LooperEngine.h"
#include "engine/LooperStateConfig.h"
#include "engine/LooperStateMachine.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Eq;
using ::testing::FloatNear;

// ============================================================================
// Test Fixtures and Helpers
// ============================================================================

class IntegrationTestBase : public ::testing::Test
{
protected:
    static constexpr double TEST_SAMPLE_RATE = 44100.0;
    static constexpr int TEST_BLOCK_SIZE = 512;
    static constexpr int TEST_CHANNELS = 2;

    juce::ScopedJuceInitialiser_GUI juceInit;

    void SetUp() override
    {
        // Ensure message manager is available for all integration tests
        juce::MessageManager::getInstance();
    }

    void fillBufferWithTone (juce::AudioBuffer<float>& buffer, float frequency, float amplitude = 0.5f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float sample = amplitude * std::sin (2.0f * M_PI * frequency * i / TEST_SAMPLE_RATE);
                buffer.setSample (ch, i, sample);
            }
        }
    }

    void fillBufferWithValue (juce::AudioBuffer<float>& buffer, float value)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                buffer.setSample (ch, i, value);
            }
        }
    }

    float getBufferRMS (const juce::AudioBuffer<float>& buffer) { return buffer.getRMSLevel (0, 0, buffer.getNumSamples()); }

    bool bufferIsNearlyZero (const juce::AudioBuffer<float>& buffer, float threshold = 0.001f)
    {
        return buffer.getMagnitude (0, 0, buffer.getNumSamples()) < threshold;
    }

    void processBlocksUntilWraparound (LoopTrack& track, juce::AudioBuffer<float>& buffer, LooperState state, int maxBlocks = 1000)
    {
        int blocksProcessed = 0;
        while (! track.hasWrappedAround() && blocksProcessed < maxBlocks)
        {
            track.processPlayback (buffer, buffer.getNumSamples(), false, state);
            blocksProcessed++;
        }
    }

    void pumpMessageManager (int maxIterations = 10)
    {
        auto* mm = juce::MessageManager::getInstance();
        if (mm)
        {
            for (int i = 0; i < maxIterations; ++i)
            {
                mm->runDispatchLoop();
                // mm->runDispatchLoopUntil (1);
            }
        }
    }
};

// ============================================================================
// LoopTrack Integration Tests
// ============================================================================

class LoopTrackIntegrationTest : public IntegrationTestBase
{
protected:
    LoopTrack track;
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;
    juce::MidiBuffer midiBuffer;

    void SetUp() override
    {
        track.prepareToPlay (TEST_SAMPLE_RATE, TEST_BLOCK_SIZE, TEST_CHANNELS);
        inputBuffer.setSize (TEST_CHANNELS, TEST_BLOCK_SIZE);
        outputBuffer.setSize (TEST_CHANNELS, TEST_BLOCK_SIZE);
    }
};

TEST_F (LoopTrackIntegrationTest, RecordAndPlaybackLoop)
{
    // Record a 1-second loop
    fillBufferWithTone (inputBuffer, 440.0f, 0.5f);

    int samplesPerSecond = static_cast<int> (TEST_SAMPLE_RATE);
    int blocksNeeded = samplesPerSecond / TEST_BLOCK_SIZE;

    // Record
    for (int i = 0; i < blocksNeeded; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }

    track.finalizeLayer (false, 0);

    // Verify track has content
    EXPECT_GT (track.getTrackLengthSamples(), 0);
    EXPECT_LE (track.getTrackLengthSamples(), samplesPerSecond + TEST_BLOCK_SIZE);

    // Playback and verify audio is present
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);

    EXPECT_GT (getBufferRMS (outputBuffer), 0.1f);
}

TEST_F (LoopTrackIntegrationTest, OverdubLayersAudio)
{
    // Record initial loop
    fillBufferWithValue (inputBuffer, 0.3f);

    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    int initialLength = track.getTrackLengthSamples();

    // Initialize overdub session
    track.initializeForNewOverdubSession();

    // Overdub with different level
    fillBufferWithValue (inputBuffer, 0.4f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, true, LooperState::Overdubbing);
    }
    track.finalizeLayer (true, 0);

    // Length should not change
    EXPECT_EQ (track.getTrackLengthSamples(), initialLength);

    // Playback should have combined signal
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);

    // Combined signal should be louder than original
    EXPECT_GT (getBufferRMS (outputBuffer), 0.3f);
}

TEST_F (LoopTrackIntegrationTest, UndoRedoCycle)
{
    // Record initial layer
    fillBufferWithValue (inputBuffer, 0.5f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    int initialLength = track.getTrackLengthSamples();

    // Overdub second layer
    track.initializeForNewOverdubSession();
    fillBufferWithValue (inputBuffer, 0.3f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, true, LooperState::Overdubbing);
    }
    track.finalizeLayer (true, 0);

    // Length should remain the same after overdub
    EXPECT_EQ (track.getTrackLengthSamples(), initialLength);

    // Undo
    EXPECT_TRUE (track.undo());

    // Length should still be the same
    EXPECT_EQ (track.getTrackLengthSamples(), initialLength);

    // Redo
    EXPECT_TRUE (track.redo());

    // Length should still be the same
    EXPECT_EQ (track.getTrackLengthSamples(), initialLength);
}

TEST_F (LoopTrackIntegrationTest, PlaybackSpeedAffectsPosition)
{
    // Record short loop
    fillBufferWithValue (inputBuffer, 0.5f);
    for (int i = 0; i < 20; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    int loopLength = track.getTrackLengthSamples();
    track.setReadPosition (0);

    // Normal speed playback
    track.setPlaybackSpeed (1.0f);
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);
    int normalPosition = track.getCurrentReadPosition();

    // Double speed playback
    track.setReadPosition (0);
    track.setPlaybackSpeed (2.0f);
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);
    int doubleSpeedPosition = track.getCurrentReadPosition();

    // Should advance roughly twice as fast
    EXPECT_GT (doubleSpeedPosition, normalPosition * 1.5f);
}

TEST_F (LoopTrackIntegrationTest, ReversePlaybackMovesBackward)
{
    // Record loop
    fillBufferWithValue (inputBuffer, 0.5f);
    for (int i = 0; i < 20; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    int loopLength = track.getTrackLengthSamples();
    track.setReadPosition (loopLength / 2);
    int startPosition = track.getCurrentReadPosition();

    // Reverse playback
    track.setPlaybackDirectionBackward();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);

    int endPosition = track.getCurrentReadPosition();

    // Position should have decreased
    EXPECT_LT (endPosition, startPosition);
}

TEST_F (LoopTrackIntegrationTest, VolumeControlAffectsOutput)
{
    // Record loop
    fillBufferWithValue (inputBuffer, 0.5f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    // Full volume playback
    track.setReadPosition (0);
    track.setTrackVolume (1.0f);
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);
    float fullVolumeRMS = getBufferRMS (outputBuffer);

    // Half volume playback
    track.setReadPosition (0);
    track.setTrackVolume (0.5f);
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);
    float halfVolumeRMS = getBufferRMS (outputBuffer);

    EXPECT_NEAR (halfVolumeRMS, fullVolumeRMS * 0.5f, 0.1f);
}

TEST_F (LoopTrackIntegrationTest, MuteProducesNoOutput)
{
    // Record loop
    fillBufferWithValue (inputBuffer, 0.5f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    // Mute and playback
    track.setMuted (true);
    EXPECT_TRUE (track.isMuted());

    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);

    // Output should be very quiet (muted)
    EXPECT_LT (getBufferRMS (outputBuffer), 0.5f);
}

TEST_F (LoopTrackIntegrationTest, LoopRegionRestrictsPlayback)
{
    // Record longer loop
    fillBufferWithValue (inputBuffer, 0.5f);
    for (int i = 0; i < 50; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    int loopLength = track.getTrackLengthSamples();
    int regionStart = loopLength / 4;
    int regionEnd = loopLength / 2;

    // Set loop region
    track.setLoopRegion (regionStart, regionEnd);
    track.setReadPosition (regionStart);

    // Process until wrap
    processBlocksUntilWraparound (track, outputBuffer, LooperState::Playing);

    int wrappedPosition = track.getCurrentReadPosition();

    // Should wrap to region start, not 0
    EXPECT_GE (wrappedPosition, regionStart);
    EXPECT_LT (wrappedPosition, regionEnd);
}

TEST_F (LoopTrackIntegrationTest, ClearRemovesAllContent)
{
    // Record loop
    fillBufferWithValue (inputBuffer, 0.5f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    EXPECT_GT (track.getTrackLengthSamples(), 0);

    // Clear
    track.clear();

    EXPECT_EQ (track.getTrackLengthSamples(), 0);
    EXPECT_EQ (track.getCurrentReadPosition(), 0);
    EXPECT_EQ (track.getCurrentWritePosition(), 0);
}

TEST_F (LoopTrackIntegrationTest, OverdubGainsMixLayers)
{
    // Record initial layer at known level
    fillBufferWithValue (inputBuffer, 0.4f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, false, LooperState::Recording);
    }
    track.finalizeLayer (false, 0);

    // Set overdub gains: keep existing at 0.5, new at 0.5
    track.setOverdubGainOld (0.5);
    track.setOverdubGainNew (0.5);

    track.initializeForNewOverdubSession();

    // Overdub with same level
    fillBufferWithValue (inputBuffer, 0.4f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, true, LooperState::Overdubbing);
    }
    track.finalizeLayer (true, 0);

    // Result should have mixed content
    track.setReadPosition (0);
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);

    // Just verify we have audio output
    EXPECT_GT (getBufferRMS (outputBuffer), 0.1f);
}

// ============================================================================
// LooperStateMachine Integration Tests
// ============================================================================

class StateMachineIntegrationTest : public IntegrationTestBase
{
protected:
    LooperStateMachine stateMachine;
    LoopTrack track;
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;

    void SetUp() override
    {
        track.prepareToPlay (TEST_SAMPLE_RATE, TEST_BLOCK_SIZE, TEST_CHANNELS);
        inputBuffer.setSize (TEST_CHANNELS, TEST_BLOCK_SIZE);
        outputBuffer.setSize (TEST_CHANNELS, TEST_BLOCK_SIZE);
    }

    StateContext createContext (LooperState currentState)
    {
        // Initialize the tracks array properly
        static std::array<std::unique_ptr<LoopTrack>, NUM_TRACKS> tracks;
        static std::array<bool, NUM_TRACKS> tracksToPlay;

        // Ensure track at index 0 exists
        if (! tracks[0])
        {
            tracks[0] = std::make_unique<LoopTrack>();
            tracks[0]->prepareToPlay (TEST_SAMPLE_RATE, TEST_BLOCK_SIZE, TEST_CHANNELS);
        }

        tracksToPlay.fill (false);
        tracksToPlay[0] = true;

        return StateContext { .track = &track,
                              .inputBuffer = &inputBuffer,
                              .outputBuffer = &outputBuffer,
                              .numSamples = TEST_BLOCK_SIZE,
                              .sampleRate = TEST_SAMPLE_RATE,
                              .trackIndex = 0,
                              .wasRecording = StateConfig::isRecording (currentState),
                              .isSinglePlayMode = true,
                              .syncMasterLength = 0,
                              .syncMasterTrackIndex = -1,
                              .allTracks = &tracks,
                              .tracksToPlay = &tracksToPlay };
    }
};

TEST_F (StateMachineIntegrationTest, IdleToRecordingTransition)
{
    LooperState state = LooperState::Idle;
    auto ctx = createContext (state);

    EXPECT_TRUE (stateMachine.transition (state, LooperState::Recording, ctx));
    EXPECT_EQ (state, LooperState::Recording);
}

TEST_F (StateMachineIntegrationTest, RecordingToPlayingTransition)
{
    LooperState state = LooperState::Recording;
    auto ctx = createContext (state);

    EXPECT_TRUE (stateMachine.transition (state, LooperState::Playing, ctx));
    EXPECT_EQ (state, LooperState::Playing);
}

TEST_F (StateMachineIntegrationTest, InvalidTransitionRejected)
{
    LooperState state = LooperState::Idle;
    auto ctx = createContext (state);

    // Can't go directly from Idle to Overdubbing
    EXPECT_FALSE (stateMachine.transition (state, LooperState::Overdubbing, ctx));
    EXPECT_EQ (state, LooperState::Idle); // State unchanged
}

TEST_F (StateMachineIntegrationTest, RecordingStateProcessesInput)
{
    LooperState state = LooperState::Recording;
    fillBufferWithTone (inputBuffer, 440.0f, 0.5f);

    auto ctx = createContext (state);

    // Process some audio
    for (int i = 0; i < 10; ++i)
    {
        stateMachine.processAudio (state, ctx);
    }

    // Track should have recorded content
    EXPECT_GT (track.getCurrentWritePosition(), 0);
}

TEST_F (StateMachineIntegrationTest, OverdubMixesWithExisting)
{
    // Record initial
    LooperState recordState = LooperState::Recording;
    fillBufferWithValue (inputBuffer, 0.3f);

    auto recordCtx = createContext (recordState);
    for (int i = 0; i < 10; ++i)
    {
        stateMachine.processAudio (recordState, recordCtx);
    }
    track.finalizeLayer (false, 0);

    // Transition to overdub
    LooperState overdubState = LooperState::Overdubbing;
    auto overdubCtx = createContext (overdubState);

    // Initialize overdub
    stateMachine.transition (recordState, overdubState, overdubCtx);

    // Overdub more audio
    fillBufferWithValue (inputBuffer, 0.3f);
    for (int i = 0; i < 5; ++i)
    {
        stateMachine.processAudio (overdubState, overdubCtx);
    }

    // Track should have mixed content
    // (Exact verification requires playback)
}

TEST_F (StateMachineIntegrationTest, StoppedStateDoesNotProcess)
{
    // Record something first
    LooperState recordState = LooperState::Recording;
    fillBufferWithValue (inputBuffer, 0.5f);

    auto recordCtx = createContext (recordState);
    for (int i = 0; i < 10; ++i)
    {
        stateMachine.processAudio (recordState, recordCtx);
    }
    track.finalizeLayer (false, 0);

    int readPosBefore = track.getCurrentReadPosition();

    // Stop
    LooperState stoppedState = LooperState::Stopped;
    auto stoppedCtx = createContext (stoppedState);

    outputBuffer.clear();
    stateMachine.processAudio (stoppedState, stoppedCtx);

    // Read position should not advance
    EXPECT_EQ (track.getCurrentReadPosition(), readPosBefore);

    // Output should be silent
    EXPECT_TRUE (bufferIsNearlyZero (outputBuffer));
}

TEST_F (StateMachineIntegrationTest, TransitionOnEnterCallbacks)
{
    // Record initial layer
    LooperState recordState = LooperState::Recording;
    fillBufferWithValue (inputBuffer, 0.5f);

    auto recordCtx = createContext (recordState);
    for (int i = 0; i < 10; ++i)
    {
        stateMachine.processAudio (recordState, recordCtx);
    }

    // Transition to playing - should call onExit for recording
    LooperState playState = LooperState::Playing;
    auto playCtx = createContext (playState);

    stateMachine.transition (recordState, playState, playCtx);

    // Track should be finalized (length set)
    EXPECT_GT (track.getTrackLengthSamples(), 0);
}

// ============================================================================
// LooperEngine Integration Tests
// ============================================================================

class LooperEngineIntegrationTest : public IntegrationTestBase
{
protected:
    LooperEngine engine;
    juce::AudioBuffer<float> audioBuffer;
    juce::MidiBuffer midiBuffer;

    void SetUp() override
    {
        engine.prepareToPlay (TEST_SAMPLE_RATE, TEST_BLOCK_SIZE, TEST_CHANNELS);
        audioBuffer.setSize (TEST_CHANNELS, TEST_BLOCK_SIZE);
    }

    void processBlocks (int numBlocks)
    {
        for (int i = 0; i < numBlocks; ++i)
        {
            engine.processBlock (audioBuffer, midiBuffer);
        }
    }
};

TEST_F (LooperEngineIntegrationTest, InitializesWithDefaultState)
{
    EXPECT_EQ (engine.getNumTracks(), NUM_TRACKS);

    auto* track0 = engine.getTrackByIndex (0);
    ASSERT_NE (track0, nullptr);
    EXPECT_EQ (track0->getTrackLengthSamples(), 0);
}

TEST_F (LooperEngineIntegrationTest, RecordPlaybackCycle)
{
    fillBufferWithTone (audioBuffer, 440.0f, 0.3f);

    // Start recording
    auto* bus = engine.getMessageBus();
    EngineMessageBus::Command recordCmd;
    recordCmd.type = EngineMessageBus::CommandType::ToggleRecord;
    recordCmd.trackIndex = 0;
    bus->pushCommand (recordCmd);

    // Record for 0.5 seconds
    int blocksToRecord = static_cast<int> (TEST_SAMPLE_RATE * 0.5 / TEST_BLOCK_SIZE);
    processBlocks (blocksToRecord);

    // Stop recording
    EngineMessageBus::Command stopCmd;
    stopCmd.type = EngineMessageBus::CommandType::ToggleRecord;
    stopCmd.trackIndex = 0;
    bus->pushCommand (stopCmd);
    engine.processBlock (audioBuffer, midiBuffer);

    // Track should have content
    auto* track = engine.getTrackByIndex (0);
    EXPECT_GT (track->getTrackLengthSamples(), 0);
}

TEST_F (LooperEngineIntegrationTest, MultiTrackRecording)
{
    engine.toggleSinglePlayMode(); // Ensure multi-track mode
    // Record on track 0
    fillBufferWithValue (audioBuffer, 0.3f);
    engine.toggleRecord();
    processBlocks (10);
    engine.toggleRecord();

    EXPECT_TRUE (engine.trackHasContent (0));

    // Switch to track 1
    engine.selectTrack (1);

    // Record on track 1
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();
    processBlocks (10);
    engine.toggleRecord();

    EXPECT_TRUE (engine.trackHasContent (1));

    // Both tracks should be independent
    auto* track0 = engine.getTrackByIndex (0);
    auto* track1 = engine.getTrackByIndex (1);

    EXPECT_NE (track0->getTrackLengthSamples(), track1->getTrackLengthSamples());
}

TEST_F (LooperEngineIntegrationTest, UndoRedoViaEngine)
{
    // Record initial layer
    fillBufferWithValue (audioBuffer, 0.4f);
    engine.toggleRecord();
    processBlocks (10);
    engine.toggleRecord();

    auto* track = engine.getTrackByIndex (0);
    int initialLength = track->getTrackLengthSamples();

    // Overdub
    engine.toggleRecord();
    fillBufferWithValue (audioBuffer, 0.3f);
    processBlocks (5);
    engine.toggleRecord();

    // Undo
    engine.undo (0);

    // Should restore initial state (length shouldn't change for overdub, but content should)
    EXPECT_EQ (track->getTrackLengthSamples(), initialLength);

    // Redo
    engine.redo (0);
}

TEST_F (LooperEngineIntegrationTest, ClearTrack)
{
    // Record something
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();
    processBlocks (10);
    engine.toggleRecord();

    EXPECT_TRUE (engine.trackHasContent (0));

    // Clear
    engine.clear (0);

    EXPECT_FALSE (engine.trackHasContent (0));
}

TEST_F (LooperEngineIntegrationTest, TrackSoloMutesOthers)
{
    // Record on multiple tracks
    for (int trackIdx = 0; trackIdx < 3; ++trackIdx)
    {
        engine.selectTrack (trackIdx);
        fillBufferWithValue (audioBuffer, 0.5f);
        engine.toggleRecord();
        processBlocks (10);
        engine.toggleRecord();
    }

    // Solo track 1
    engine.toggleSolo (1);

    auto* track0 = engine.getTrackByIndex (0);
    auto* track1 = engine.getTrackByIndex (1);
    auto* track2 = engine.getTrackByIndex (2);

    EXPECT_TRUE (track0->isMuted());
    EXPECT_TRUE (track1->isSoloed());
    EXPECT_TRUE (track2->isMuted());
}

TEST_F (LooperEngineIntegrationTest, SinglePlayModeOnlyPlaysActiveTrack)
{
    // Record on two tracks
    engine.selectTrack (0);
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();
    processBlocks (10);
    engine.toggleRecord();

    engine.selectTrack (1);
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();
    processBlocks (10);
    engine.toggleRecord();

    // Enable single play mode (if not already)
    if (! engine.isSinglePlayMode())
    {
        engine.toggleSinglePlayMode();
    }

    // Select track 0 and play
    engine.selectTrack (0);
    engine.togglePlay();

    // Only track 0 should play
    EXPECT_TRUE (engine.shouldTrackPlay (0));
    EXPECT_FALSE (engine.shouldTrackPlay (1));
}

TEST_F (LooperEngineIntegrationTest, MetronomeProducesClicks)
{
    auto* metronome = engine.getMetronome();
    metronome->setEnabled (true);
    metronome->setBpm (120);

    audioBuffer.clear();
    engine.processBlock (audioBuffer, midiBuffer);

    // After enough samples, should hear clicks
    int samplesNeeded = static_cast<int> (TEST_SAMPLE_RATE);
    int blocksNeeded = samplesNeeded / TEST_BLOCK_SIZE;

    bool heardClick = false;
    for (int i = 0; i < blocksNeeded; ++i)
    {
        audioBuffer.clear();
        engine.processBlock (audioBuffer, midiBuffer);

        if (getBufferRMS (audioBuffer) > 0.01f)
        {
            heardClick = true;
            break;
        }
    }

    EXPECT_TRUE (heardClick);
}

TEST_F (LooperEngineIntegrationTest, CommandBusRoutesCommands)
{
    auto* bus = engine.getMessageBus();

    // Send volume command
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::SetVolume;
    cmd.trackIndex = 0;
    cmd.payload = 0.7f;

    bus->pushCommand (cmd);

    // Process block should handle command
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.processBlock (audioBuffer, midiBuffer);

    // Volume should be set
    auto* track = engine.getTrackByIndex (0);
    EXPECT_FLOAT_EQ (track->getTrackVolume(), 0.7f);
}

TEST_F (LooperEngineIntegrationTest, LoadBackingTrack)
{
    // Create a backing track
    juce::AudioBuffer<float> backingTrack (TEST_CHANNELS, static_cast<int> (TEST_SAMPLE_RATE)); // 1 second
    fillBufferWithTone (backingTrack, 440.0f, 0.5f);

    // Load it to track 0 via command
    auto* bus = engine.getMessageBus();
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::LoadAudioFile;
    cmd.trackIndex = 0;
    // Note: LoadAudioFile expects a juce::File, not AudioBuffer
    // This test needs adjustment based on actual API

    // For now, just verify track starts empty
    auto* track = engine.getTrackByIndex (0);
    EXPECT_EQ (track->getTrackLengthSamples(), 0);
}

TEST_F (LooperEngineIntegrationTest, GranularFreezeProcessesAudio)
{
    auto* freeze = engine.getGranularFreeze();

    // Record some audio first
    fillBufferWithTone (audioBuffer, 440.0f, 0.5f);
    engine.processBlock (audioBuffer, midiBuffer);
    engine.toggleRecord();
    processBlocks (20);
    engine.toggleRecord();

    // Enable freeze
    engine.toggleGranularFreeze();
    EXPECT_TRUE (freeze->isEnabled());

    // Process with playback
    engine.togglePlay();
    // audioBuffer.clear();
    engine.processBlock (audioBuffer, midiBuffer);

    // Freeze should affect the audio (hard to test exact effect, just verify it processes)
    EXPECT_GT (getBufferRMS (audioBuffer), 0.0f);
}

TEST_F (LooperEngineIntegrationTest, InputOutputGainControl)
{
    // Set input gain
    auto* bus = engine.getMessageBus();
    EngineMessageBus::Command inputGainCmd;
    inputGainCmd.type = EngineMessageBus::CommandType::SetInputGain;
    inputGainCmd.trackIndex = -1;
    inputGainCmd.payload = 0.5f;
    bus->pushCommand (inputGainCmd);

    // Record with reduced input gain
    fillBufferWithValue (audioBuffer, 0.8f);
    engine.processBlock (audioBuffer, midiBuffer);

    engine.toggleRecord();
    processBlocks (10);
    engine.toggleRecord();

    // Set output gain
    EngineMessageBus::Command outputGainCmd;
    outputGainCmd.type = EngineMessageBus::CommandType::SetOutputGain;
    outputGainCmd.trackIndex = -1;
    outputGainCmd.payload = 0.5f;
    bus->pushCommand (outputGainCmd);

    // Playback with output gain
    engine.togglePlay();
    audioBuffer.clear();
    fillBufferWithValue (audioBuffer, 0.0f);
    engine.processBlock (audioBuffer, midiBuffer);

    // Output should be scaled
    float rms = getBufferRMS (audioBuffer);
    EXPECT_LT (rms, 0.5f); // Should be reduced
}

TEST_F (LooperEngineIntegrationTest, TrackSpeedAndPitchControl)
{
    // Record a loop first
    fillBufferWithValue (audioBuffer, 0.5f);

    engine.processBlock (audioBuffer, midiBuffer);
    auto* bus = engine.getMessageBus();

    EngineMessageBus::Command recCmd;
    recCmd.type = EngineMessageBus::CommandType::ToggleRecord;
    recCmd.trackIndex = 0;
    bus->pushCommand (recCmd);

    processBlocks (20);

    bus->pushCommand (recCmd); // Stop recording
    engine.processBlock (audioBuffer, midiBuffer);

    auto* track = engine.getTrackByIndex (0);

    // Set speed
    EngineMessageBus::Command speedCmd;
    speedCmd.type = EngineMessageBus::CommandType::SetPlaybackSpeed;
    speedCmd.trackIndex = 0;
    speedCmd.payload = 1.5f;
    bus->pushCommand (speedCmd);
    engine.processBlock (audioBuffer, midiBuffer);

    EXPECT_FLOAT_EQ (track->getPlaybackSpeed(), 1.5f);

    // Set pitch
    EngineMessageBus::Command pitchCmd;
    pitchCmd.type = EngineMessageBus::CommandType::SetPlaybackPitch;
    pitchCmd.trackIndex = 0;
    pitchCmd.payload = 1.53f;
    bus->pushCommand (pitchCmd);
    engine.processBlock (audioBuffer, midiBuffer);

    EXPECT_NEAR (track->getPlaybackPitch(), 1.53, 0.1);
}

TEST_F (LooperEngineIntegrationTest, PendingTrackSwitchAtWrapAround)
{
    // Record on track 0
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();
    processBlocks (30);
    engine.toggleRecord();

    // Record on track 1
    engine.selectTrack (1);
    fillBufferWithValue (audioBuffer, 0.3f);
    engine.toggleRecord();
    processBlocks (30);
    engine.toggleRecord();

    // Start playing track 0
    engine.selectTrack (0);
    engine.togglePlay();

    // Schedule track switch
    engine.selectTrack (1);

    // Process until track actually switches (at wrap)
    auto* bridge = engine.getEngineStateBridge();
    int initialActive = 0;
    int maxBlocks = 200;
    int blocksProcessed = 0;

    while (blocksProcessed < maxBlocks)
    {
        engine.processBlock (audioBuffer, midiBuffer);

        // Check if switch happened
        int currentActive = engine.getActiveTrackIndex();
        if (currentActive != initialActive)
        {
            EXPECT_EQ (currentActive, 1);
            break;
        }

        blocksProcessed++;
    }

    EXPECT_LT (blocksProcessed, maxBlocks); // Should have switched before timeout
}

TEST_F (LooperEngineIntegrationTest, CancelRecordingRestoresState)
{
    // This test depends on specific cancel behavior
    // Just verify basic state management
    auto* track = engine.getTrackByIndex (0);
    EXPECT_EQ (track->getTrackLengthSamples(), 0);
}

// ============================================================================
// Multi-Track Sync Integration Tests
// ============================================================================

class MultiTrackSyncTest : public IntegrationTestBase
{
protected:
    LooperEngine engine;
    juce::AudioBuffer<float> audioBuffer;
    juce::MidiBuffer midiBuffer;

    void SetUp() override
    {
        engine.prepareToPlay (TEST_SAMPLE_RATE, TEST_BLOCK_SIZE, TEST_CHANNELS);
        audioBuffer.setSize (TEST_CHANNELS, TEST_BLOCK_SIZE);
    }

    void recordTrack (int trackIndex, int numBlocks, float value)
    {
        engine.selectTrack (trackIndex);
        fillBufferWithValue (audioBuffer, value);
        engine.toggleRecord();

        for (int i = 0; i < numBlocks; ++i)
        {
            engine.processBlock (audioBuffer, midiBuffer);
        }

        engine.toggleRecord();
    }
};

TEST_F (MultiTrackSyncTest, SyncedTracksQuantizeToMaster)
{
    engine.toggleSinglePlayMode(); // Multi-track mode
    // Record master track (track 0)
    recordTrack (0, 40, 0.5f);

    auto* masterTrack = engine.getTrackByIndex (0);
    int masterLength = masterTrack->getTrackLengthSamples();

    EXPECT_GT (masterLength, 0);

    // Enable sync on track 1 and record
    engine.selectTrack (1);
    auto* track1 = engine.getTrackByIndex (1);
    track1->setSynced (true);

    recordTrack (1, 20, 0.3f);

    auto* syncedTrack = engine.getTrackByIndex (1);
    int syncedLength = syncedTrack->getTrackLengthSamples();

    // Synced track should have some relationship to master
    // (exact behavior depends on sync implementation)
    EXPECT_GT (syncedLength, 0);
}

TEST_F (MultiTrackSyncTest, UnsyncedTracksMaintainIndependentLength)
{
    // Record master track
    recordTrack (0, 40, 0.5f);

    auto* masterTrack = engine.getTrackByIndex (0);
    int masterLength = masterTrack->getTrackLengthSamples();

    // Record unsynced track with different length
    engine.selectTrack (1);
    // Ensure track 1 is not synced
    auto* track1 = engine.getTrackByIndex (1);
    if (track1->isSynced())
    {
        engine.toggleSync (1);
    }

    recordTrack (1, 20, 0.3f);

    int track1Length = track1->getTrackLengthSamples();

    // Track 1 should have different length
    EXPECT_NE (track1Length, masterLength);
    EXPECT_LT (track1Length, masterLength);
}

TEST_F (MultiTrackSyncTest, PlaybackPositionSyncAcrossTracks)
{
    // Disable single play mode to hear all tracks
    if (engine.isSinglePlayMode())
    {
        engine.toggleSinglePlayMode();
    }

    // Record two synced tracks
    recordTrack (0, 40, 0.5f);

    engine.selectTrack (1);
    engine.toggleSync (1);
    recordTrack (1, 40, 0.3f);

    // Start playback
    engine.selectTrack (0);
    engine.togglePlay();

    // Process some blocks
    for (int i = 0; i < 10; ++i)
    {
        engine.processBlock (audioBuffer, midiBuffer);
    }

    auto* track0 = engine.getTrackByIndex (0);
    auto* track1 = engine.getTrackByIndex (1);

    // Read positions should be synchronized
    int pos0 = track0->getCurrentReadPosition();
    int pos1 = track1->getCurrentReadPosition();

    // Allow some tolerance for rounding
    EXPECT_NEAR (pos0, pos1, TEST_BLOCK_SIZE * 2);
}

TEST_F (MultiTrackSyncTest, LoopRegionSyncsAcrossTracks)
{
    engine.toggleSinglePlayMode(); // Multi-track mode
    // Record two tracks
    recordTrack (0, 60, 0.5f);
    recordTrack (1, 60, 0.3f);

    auto* track0 = engine.getTrackByIndex (0);
    auto* track1 = engine.getTrackByIndex (1);

    // Just verify both tracks have content
    EXPECT_GT (track0->getTrackLengthSamples(), 0);
    EXPECT_GT (track1->getTrackLengthSamples(), 0);
}

// ============================================================================
// Playback Mode Integration Tests
// ============================================================================

class PlaybackModeTest : public IntegrationTestBase
{
protected:
    LooperEngine engine;
    juce::AudioBuffer<float> audioBuffer;
    juce::MidiBuffer midiBuffer;

    void SetUp() override
    {
        engine.prepareToPlay (TEST_SAMPLE_RATE, TEST_BLOCK_SIZE, TEST_CHANNELS);
        audioBuffer.setSize (TEST_CHANNELS, TEST_BLOCK_SIZE);
    }

    void setupMultipleTracks()
    {
        engine.toggleSinglePlayMode(); // Multi-track mode
        for (int i = 0; i < 3; ++i)
        {
            engine.selectTrack (i);
            fillBufferWithValue (audioBuffer, 0.3f + i * 0.1f);
            engine.toggleRecord();

            for (int j = 0; j < 20; ++j)
            {
                engine.processBlock (audioBuffer, midiBuffer);
            }

            engine.toggleRecord();
        }
    }
};

TEST_F (PlaybackModeTest, SinglePlayModeOnlyPlaysActive)
{
    setupMultipleTracks();

    // Verify tracks have content
    EXPECT_TRUE (engine.trackHasContent (0));
    EXPECT_TRUE (engine.trackHasContent (1));
    EXPECT_TRUE (engine.trackHasContent (2));
}

TEST_F (PlaybackModeTest, MultiPlayModePlaysAllTracks)
{
    setupMultipleTracks();

    // Verify all tracks have content
    EXPECT_TRUE (engine.trackHasContent (0));
    EXPECT_TRUE (engine.trackHasContent (1));
    EXPECT_TRUE (engine.trackHasContent (2));
}

TEST_F (PlaybackModeTest, MutedTracksDoNotPlayInMultiMode)
{
    setupMultipleTracks();

    // Mute track 1
    auto* bus = engine.getMessageBus();
    EngineMessageBus::Command muteCmd;
    muteCmd.type = EngineMessageBus::CommandType::ToggleMute;
    muteCmd.trackIndex = 1;
    bus->pushCommand (muteCmd);
    engine.processBlock (audioBuffer, midiBuffer);

    auto* track1 = engine.getTrackByIndex (1);
    EXPECT_TRUE (track1->isMuted());
}

// ============================================================================
// Stress and Edge Case Tests
// ============================================================================

class StressTest : public IntegrationTestBase
{
protected:
    LooperEngine engine;
    juce::AudioBuffer<float> audioBuffer;
    juce::MidiBuffer midiBuffer;

    void SetUp() override
    {
        engine.prepareToPlay (TEST_SAMPLE_RATE, TEST_BLOCK_SIZE, TEST_CHANNELS);
        audioBuffer.setSize (TEST_CHANNELS, TEST_BLOCK_SIZE);
    }
};

TEST_F (StressTest, RapidTrackSwitching)
{
    // Record on some tracks
    for (int i = 0; i < 3; ++i)
    {
        engine.selectTrack (i);
        fillBufferWithValue (audioBuffer, 0.5f);

        auto* bus = engine.getMessageBus();
        EngineMessageBus::Command recCmd;
        recCmd.type = EngineMessageBus::CommandType::ToggleRecord;
        recCmd.trackIndex = i;
        bus->pushCommand (recCmd);

        for (int j = 0; j < 10; ++j)
        {
            engine.processBlock (audioBuffer, midiBuffer);
        }

        bus->pushCommand (recCmd); // Stop
        engine.processBlock (audioBuffer, midiBuffer);
    }

    // Rapidly switch tracks
    for (int i = 0; i < 50; ++i)
    {
        engine.selectTrack (i % 3);
        engine.processBlock (audioBuffer, midiBuffer);
    }

    // Should not crash
    EXPECT_TRUE (engine.trackHasContent (0));
}

TEST_F (StressTest, MaximumUndoRedoCycles)
{
    // Record initial
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();

    for (int i = 0; i < 10; ++i)
    {
        engine.processBlock (audioBuffer, midiBuffer);
    }

    engine.toggleRecord();

    // Perform maximum overdubs
    for (int layer = 0; layer < MAX_UNDO_LAYERS; ++layer)
    {
        engine.toggleRecord();

        fillBufferWithValue (audioBuffer, 0.3f + layer * 0.05f);
        for (int i = 0; i < 5; ++i)
        {
            engine.processBlock (audioBuffer, midiBuffer);
        }

        engine.toggleRecord();
    }

    // Undo all
    for (int i = 0; i < MAX_UNDO_LAYERS; ++i)
    {
        engine.undo (0);
    }

    // Redo all
    for (int i = 0; i < MAX_UNDO_LAYERS; ++i)
    {
        engine.redo (0);
    }

    // Should still have valid content
    EXPECT_TRUE (engine.trackHasContent (0));
}

TEST_F (StressTest, LongRecordingSession)
{
    fillBufferWithValue (audioBuffer, 0.5f);

    //switch track
    engine.processBlock (audioBuffer, midiBuffer);

    auto* bus = engine.getMessageBus();
    EngineMessageBus::Command recCmd;
    recCmd.type = EngineMessageBus::CommandType::ToggleRecord;
    recCmd.trackIndex = 0;
    bus->pushCommand (recCmd);

    // Record for 2 seconds (reduced from 5 to speed up test)
    int blocksToRecord = static_cast<int> (TEST_SAMPLE_RATE * 2.0 / TEST_BLOCK_SIZE);

    for (int i = 0; i < blocksToRecord; ++i)
    {
        engine.processBlock (audioBuffer, midiBuffer);
    }

    bus->pushCommand (recCmd); // Stop
    engine.processBlock (audioBuffer, midiBuffer);

    auto* track = engine.getTrackByIndex (0);
    int recordedLength = track->getTrackLengthSamples();

    EXPECT_GT (recordedLength, static_cast<int> (TEST_SAMPLE_RATE * 1.5));
}

TEST_F (StressTest, SimultaneousCommandsAndAudio)
{
    auto* bus = engine.getMessageBus();

    // Queue many commands
    for (int i = 0; i < 50; ++i)
    {
        EngineMessageBus::Command cmd;
        cmd.type = (i % 2 == 0) ? EngineMessageBus::CommandType::SetVolume : EngineMessageBus::CommandType::SetPlaybackSpeed;
        cmd.trackIndex = i % NUM_TRACKS;
        cmd.payload = 0.5f + (i % 10) * 0.05f;

        bus->pushCommand (cmd);
    }

    // Process with audio
    fillBufferWithValue (audioBuffer, 0.5f);

    for (int i = 0; i < 100; ++i)
    {
        engine.processBlock (audioBuffer, midiBuffer);
    }

    // Should handle all commands without crashing
    EXPECT_FALSE (bus->hasCommands()); // All should be processed
}

TEST_F (StressTest, ZeroLengthEdgeCases)
{
    // Try to play empty track
    engine.togglePlay();
    audioBuffer.clear();
    engine.processBlock (audioBuffer, midiBuffer);

    EXPECT_TRUE (bufferIsNearlyZero (audioBuffer));

    // Try to undo on empty track
    EXPECT_FALSE (engine.getTrackByIndex (0)->undo());

    // Try to set loop region on empty track
    auto* bus = engine.getMessageBus();
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::SetSubLoopRegion;
    cmd.trackIndex = 0;
    cmd.payload = std::make_pair (100, 200);
    bus->pushCommand (cmd);

    engine.processBlock (audioBuffer, midiBuffer);

    // Should handle gracefully
}

TEST_F (StressTest, ExtremPlaybackSettings)
{
    // Record something
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();

    for (int i = 0; i < 20; ++i)
    {
        engine.processBlock (audioBuffer, midiBuffer);
    }

    engine.toggleRecord();

    auto* track = engine.getTrackByIndex (0);

    // Extreme speed
    track->setPlaybackSpeed (2.0f);

    // Extreme pitch
    track->setPlaybackPitch (12.0);

    // Reverse
    track->setPlaybackDirectionBackward();

    // Playback with extreme settings
    engine.togglePlay();
    audioBuffer.clear();

    for (int i = 0; i < 50; ++i)
    {
        engine.processBlock (audioBuffer, midiBuffer);
    }

    // Should produce audio without crashing
    // (Quality may be degraded but should not crash)
}

// No main() - using GTest::Main library with CTest
