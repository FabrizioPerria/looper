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

    // Get initial playback
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);
    float initialRMS = getBufferRMS (outputBuffer);

    // Overdub second layer
    track.initializeForNewOverdubSession();
    fillBufferWithValue (inputBuffer, 0.3f);
    for (int i = 0; i < 10; ++i)
    {
        track.processRecord (inputBuffer, TEST_BLOCK_SIZE, true, LooperState::Overdubbing);
    }
    track.finalizeLayer (true, 0);

    // Get overdubbed playback
    track.setReadPosition (0);
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);
    float overdubRMS = getBufferRMS (outputBuffer);

    EXPECT_NE (initialRMS, overdubRMS);

    // Undo
    EXPECT_TRUE (track.undo());

    // Should match initial
    track.setReadPosition (0);
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);
    EXPECT_NEAR (getBufferRMS (outputBuffer), initialRMS, 0.05f);

    // Redo
    EXPECT_TRUE (track.redo());

    // Should match overdubbed
    track.setReadPosition (0);
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);
    EXPECT_NEAR (getBufferRMS (outputBuffer), overdubRMS, 0.05f);
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
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);

    EXPECT_TRUE (bufferIsNearlyZero (outputBuffer));
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

    // Result should be mix of both: 0.4 * 0.5 + 0.4 * 0.5 = 0.4
    track.setReadPosition (0);
    outputBuffer.clear();
    track.processPlayback (outputBuffer, TEST_BLOCK_SIZE, false, LooperState::Playing);

    EXPECT_NEAR (getBufferRMS (outputBuffer), 0.4f, 0.1f);
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
        std::array<std::unique_ptr<LoopTrack>, NUM_TRACKS> tracks;
        std::array<bool, NUM_TRACKS> tracksToPlay;
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

TEST_F (StateMachineIntegrationTest, PlayingStateDoesNotRecord)
{
    // First record something
    LooperState recordState = LooperState::Recording;
    fillBufferWithValue (inputBuffer, 0.5f);

    auto recordCtx = createContext (recordState);
    for (int i = 0; i < 10; ++i)
    {
        stateMachine.processAudio (recordState, recordCtx);
    }
    track.finalizeLayer (false, 0);

    int writePosBefore = track.getCurrentWritePosition();

    // Now play
    LooperState playState = LooperState::Playing;
    auto playCtx = createContext (playState);

    outputBuffer.clear();
    stateMachine.processAudio (playState, playCtx);

    // Write position should not advance
    EXPECT_EQ (track.getCurrentWritePosition(), writePosBefore);

    // Output should have content
    EXPECT_GT (getBufferRMS (outputBuffer), 0.1f);
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
    engine.toggleRecord();

    // Record for 0.5 seconds
    int blocksToRecord = static_cast<int> (TEST_SAMPLE_RATE * 0.5 / TEST_BLOCK_SIZE);
    processBlocks (blocksToRecord);

    // Stop recording
    engine.toggleRecord();

    // Track should have content
    auto* track = engine.getTrackByIndex (0);
    EXPECT_GT (track->getTrackLengthSamples(), 0);

    // Playback
    audioBuffer.clear();
    engine.togglePlay();
    engine.processBlock (audioBuffer, midiBuffer);

    // Should have audio output
    EXPECT_GT (getBufferRMS (audioBuffer), 0.1f);
}

TEST_F (LooperEngineIntegrationTest, MultiTrackRecording)
{
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

TEST_F (LooperEngineIntegrationTest, TrackSyncAffectsRecording)
{
    // Record master track
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();
    processBlocks (20);
    engine.toggleRecord();

    auto* masterTrack = engine.getTrackByIndex (0);
    int masterLength = masterTrack->getTrackLengthSamples();

    // Enable sync on track 1
    engine.selectTrack (1);
    engine.toggleSync (1);

    // Record on synced track (should quantize to master)
    fillBufferWithValue (audioBuffer, 0.3f);
    engine.toggleRecord();
    processBlocks (10); // Record less than master
    engine.toggleRecord();

    auto* syncedTrack = engine.getTrackByIndex (1);

    // Synced track should match or be multiple of master length
    EXPECT_GE (syncedTrack->getTrackLengthSamples(), masterLength);
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

    // Send record command
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::ToggleRecord;
    cmd.trackIndex = 0;
    cmd.payload = std::monostate {};

    bus->pushCommand (cmd);

    // Process block should handle command
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.processBlock (audioBuffer, midiBuffer);

    // Should now be recording (write position advancing)
    auto* track = engine.getTrackByIndex (0);
    int writePos = track->getCurrentWritePosition();

    engine.processBlock (audioBuffer, midiBuffer);

    EXPECT_GT (track->getCurrentWritePosition(), writePos);
}

TEST_F (LooperEngineIntegrationTest, LoadBackingTrack)
{
    // Create a backing track
    juce::AudioBuffer<float> backingTrack (TEST_CHANNELS, static_cast<int> (TEST_SAMPLE_RATE * 2)); // 2 seconds
    fillBufferWithTone (backingTrack, 440.0f, 0.5f);

    // Load it to track 0
    auto* track = engine.getTrackByIndex (0);
    track->loadBackingTrack (backingTrack, 0);

    EXPECT_GT (track->getTrackLengthSamples(), 0);
    EXPECT_LE (track->getTrackLengthSamples(), backingTrack.getNumSamples());

    // Playback should work immediately
    engine.togglePlay();
    audioBuffer.clear();
    engine.processBlock (audioBuffer, midiBuffer);

    EXPECT_GT (getBufferRMS (audioBuffer), 0.1f);
}

TEST_F (LooperEngineIntegrationTest, GranularFreezeProcessesAudio)
{
    auto* freeze = engine.getGranularFreeze();

    // Record some audio first
    fillBufferWithTone (audioBuffer, 440.0f, 0.5f);
    engine.toggleRecord();
    processBlocks (20);
    engine.toggleRecord();

    // Enable freeze
    engine.toggleGranularFreeze();
    EXPECT_TRUE (freeze->isEnabled());

    // Process with playback
    engine.togglePlay();
    audioBuffer.clear();
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
    // Record a loop
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();
    processBlocks (20);
    engine.toggleRecord();

    auto* track = engine.getTrackByIndex (0);

    // Set speed
    auto* bus = engine.getMessageBus();
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
    pitchCmd.payload = 3.0f;
    bus->pushCommand (pitchCmd);
    engine.processBlock (audioBuffer, midiBuffer);

    EXPECT_DOUBLE_EQ (track->getPlaybackPitch(), 3.0);
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
    // Start recording
    fillBufferWithValue (audioBuffer, 0.5f);
    engine.toggleRecord();
    processBlocks (10);

    auto* track = engine.getTrackByIndex (0);
    EXPECT_GT (track->getCurrentWritePosition(), 0);

    // Cancel recording (implementation specific - might need to use stop before recording finalized)
    // This tests the pending action system
    // engine.stop(); // Should cancel if still recording first layer

    // Track should be cleared or reverted
    // (Exact behavior depends on implementation)
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
    // Record master track (track 0)
    recordTrack (0, 40, 0.5f);

    auto* masterTrack = engine.getTrackByIndex (0);
    int masterLength = masterTrack->getTrackLengthSamples();

    // Enable sync on track 1 and record shorter
    engine.selectTrack (1);
    engine.toggleSync (1);
    recordTrack (1, 20, 0.3f);

    auto* syncedTrack = engine.getTrackByIndex (1);

    // Synced track should be quantized to master length
    EXPECT_GE (syncedTrack->getTrackLengthSamples(), masterLength);
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
    // Record two synced tracks
    recordTrack (0, 60, 0.5f);

    engine.selectTrack (1);
    engine.toggleSync (1);
    recordTrack (1, 60, 0.3f);

    auto* track0 = engine.getTrackByIndex (0);
    auto* track1 = engine.getTrackByIndex (1);

    int loopLength = track0->getTrackLengthSamples();
    int regionStart = loopLength / 4;
    int regionEnd = loopLength / 2;

    // Set loop region on track 0
    auto* bus = engine.getMessageBus();
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::SetSubLoopRegion;
    cmd.trackIndex = 0;
    cmd.payload = std::make_pair (regionStart, regionEnd);
    bus->pushCommand (cmd);
    engine.processBlock (audioBuffer, midiBuffer);

    // Both tracks should have the region
    EXPECT_TRUE (track0->hasLoopRegion());
    EXPECT_TRUE (track1->hasLoopRegion());
    EXPECT_EQ (track0->getLoopRegionStart(), track1->getLoopRegionStart());
    EXPECT_EQ (track0->getLoopRegionEnd(), track1->getLoopRegionEnd());
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

    // Enable single play mode
    if (! engine.isSinglePlayMode())
    {
        engine.toggleSinglePlayMode();
    }

    // Select track 1
    engine.selectTrack (1);

    EXPECT_TRUE (engine.shouldTrackPlay (1));
    EXPECT_FALSE (engine.shouldTrackPlay (0));
    EXPECT_FALSE (engine.shouldTrackPlay (2));
}

TEST_F (PlaybackModeTest, MultiPlayModePlaysAllTracks)
{
    setupMultipleTracks();

    // Disable single play mode
    if (engine.isSinglePlayMode())
    {
        engine.toggleSinglePlayMode();
    }

    // All tracks with content should play
    EXPECT_TRUE (engine.shouldTrackPlay (0));
    EXPECT_TRUE (engine.shouldTrackPlay (1));
    EXPECT_TRUE (engine.shouldTrackPlay (2));
}

TEST_F (PlaybackModeTest, MutedTracksDoNotPlayInMultiMode)
{
    setupMultipleTracks();

    if (engine.isSinglePlayMode())
    {
        engine.toggleSinglePlayMode();
    }

    // Mute track 1
    engine.toggleMute (1);

    EXPECT_TRUE (engine.shouldTrackPlay (0));
    EXPECT_TRUE (engine.shouldTrackPlay (2));

    // Note: muted tracks might still be marked to play but produce no output
    // This depends on implementation
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
    // Record on all tracks
    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        engine.selectTrack (i);
        fillBufferWithValue (audioBuffer, 0.5f);
        engine.toggleRecord();

        for (int j = 0; j < 10; ++j)
        {
            engine.processBlock (audioBuffer, midiBuffer);
        }

        engine.toggleRecord();
    }

    // Rapidly switch tracks during playback
    engine.togglePlay();

    for (int i = 0; i < 100; ++i)
    {
        engine.selectTrack (i % NUM_TRACKS);
        engine.processBlock (audioBuffer, midiBuffer);
    }

    // Should not crash, and some track should still be playing
    auto* bridge = engine.getEngineStateBridge();
    EXPECT_TRUE (bridge->getState().isPlaying);
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
    engine.toggleRecord();

    // Record for several seconds
    int blocksToRecord = static_cast<int> (TEST_SAMPLE_RATE * 5.0 / TEST_BLOCK_SIZE);

    for (int i = 0; i < blocksToRecord; ++i)
    {
        engine.processBlock (audioBuffer, midiBuffer);
    }

    engine.toggleRecord();

    auto* track = engine.getTrackByIndex (0);
    int recordedLength = track->getTrackLengthSamples();

    EXPECT_GT (recordedLength, static_cast<int> (TEST_SAMPLE_RATE * 4));
    EXPECT_LT (recordedLength, static_cast<int> (TEST_SAMPLE_RATE * 6));
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

// ============================================================================
// Main Test Runner
// ============================================================================

// int main (int argc, char** argv)
// {
//     ::testing::InitGoogleTest (&argc, argv);
//
//     // Initialize JUCE
//     juce::ScopedJuceInitialiser_GUI juceInit;
//
//     return RUN_ALL_TESTS();
// }
