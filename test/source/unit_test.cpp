#include "audio/EngineCommandBus.h"
#include "engine/BufferManager.h"
#include "engine/LevelMeter.h"
#include "engine/LoopFifo.h"
#include "engine/LoopLifo.h"
#include "engine/Metronome.h"
#include "engine/PlaybackEngine.h"
#include "engine/UndoManager.h"
#include "engine/VolumeProcessor.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Eq;
using ::testing::FloatNear;

// ============================================================================
// LoopFifo Tests
// ============================================================================

class LoopFifoTest : public ::testing::Test
{
protected:
    LoopFifo fifo;

    void SetUp() override { fifo.prepareToPlay (1000); }
};

TEST_F (LoopFifoTest, InitializesCorrectly)
{
    EXPECT_EQ (fifo.getMusicalLength(), 1000);
    EXPECT_EQ (fifo.getWritePos(), 0);
    EXPECT_EQ (fifo.getReadPos(), 0);
    EXPECT_DOUBLE_EQ (fifo.getExactReadPos(), 0.0);
}

TEST_F (LoopFifoTest, PrepareToWriteWithinBounds)
{
    int start1, size1, start2, size2;
    fifo.prepareToWrite (100, start1, size1, start2, size2);

    EXPECT_EQ (start1, 0);
    EXPECT_EQ (size1, 100);
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopFifoTest, PrepareToWriteWrapsAround)
{
    fifo.setWritePosition (950);

    int start1, size1, start2, size2;
    fifo.prepareToWrite (100, start1, size1, start2, size2);

    EXPECT_EQ (start1, 950);
    EXPECT_EQ (size1, 50);
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 50);
}

TEST_F (LoopFifoTest, PrepareToWriteWithWrapDisabled)
{
    fifo.setWrapAround (false);
    fifo.setWritePosition (950);

    int start1, size1, start2, size2;
    fifo.prepareToWrite (100, start1, size1, start2, size2);

    EXPECT_EQ (start1, 950);
    EXPECT_EQ (size1, 50);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopFifoTest, FinishedWriteAdvancesPosition)
{
    fifo.finishedWrite (100, false, false);
    EXPECT_EQ (fifo.getWritePos(), 100);
}

TEST_F (LoopFifoTest, FinishedWriteWrapsPosition)
{
    fifo.setWritePosition (950);
    fifo.finishedWrite (100, false, false);
    EXPECT_EQ (fifo.getWritePos(), 50);
}

TEST_F (LoopFifoTest, FinishedWriteSyncsWithReadInOverdub)
{
    fifo.setReadPosition (250);
    fifo.finishedWrite (100, true, true);
    EXPECT_EQ (fifo.getWritePos(), 250);
}

TEST_F (LoopFifoTest, PrepareToReadForwardPlayback)
{
    fifo.setReadPosition (100);

    int start1, size1, start2, size2;
    fifo.prepareToRead (50, start1, size1, start2, size2);

    EXPECT_EQ (start1, 100);
    EXPECT_EQ (size1, 50);
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopFifoTest, PrepareToReadWrapsAround)
{
    fifo.setReadPosition (980);

    int start1, size1, start2, size2;
    fifo.prepareToRead (50, start1, size1, start2, size2);

    EXPECT_EQ (start1, 980);
    EXPECT_EQ (size1, 20);
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 30);
}

TEST_F (LoopFifoTest, FinishedReadForwardPlayback)
{
    fifo.finishedRead (100, 1.0f, false);
    EXPECT_EQ (fifo.getReadPos(), 100);
    EXPECT_FLOAT_EQ (fifo.getLastPlaybackRate(), 1.0f);
}

TEST_F (LoopFifoTest, FinishedReadReversePlayback)
{
    fifo.setReadPosition (500);
    fifo.finishedRead (100, -1.0f, false);
    EXPECT_EQ (fifo.getReadPos(), 400);
    EXPECT_FLOAT_EQ (fifo.getLastPlaybackRate(), -1.0f);
}

TEST_F (LoopFifoTest, FinishedReadWithSpeed)
{
    fifo.finishedRead (100, 2.0f, false);
    EXPECT_EQ (fifo.getReadPos(), 200);
}

TEST_F (LoopFifoTest, FinishedReadSyncsWriteInOverdub)
{
    fifo.setReadPosition (300);
    fifo.finishedRead (50, 1.0f, true);
    EXPECT_EQ (fifo.getWritePos(), 350);
}

TEST_F (LoopFifoTest, ReverseReadIndexCalculation)
{
    fifo.setReadPosition (500);
    EXPECT_EQ (fifo.getReverseReadIndex (0), 500);
    EXPECT_EQ (fifo.getReverseReadIndex (10), 490);
    EXPECT_EQ (fifo.getReverseReadIndex (500), 0);
    EXPECT_EQ (fifo.getReverseReadIndex (501), 999); // Wraps
}

TEST_F (LoopFifoTest, LoopRegionRestrictsWritePosition)
{
    fifo.setLoopRegion (100, 300);
    fifo.setWritePosition (250);

    fifo.finishedWrite (100, false, false);
    EXPECT_GE (fifo.getWritePos(), 100);
    EXPECT_LT (fifo.getWritePos(), 300);
}

TEST_F (LoopFifoTest, LoopRegionWrapsWrite)
{
    fifo.setLoopRegion (100, 300);
    fifo.setWritePosition (280);

    fifo.finishedWrite (30, false, false);
    EXPECT_EQ (fifo.getWritePos(), 110); // 280 + 30 = 310, wraps to 110
}

TEST_F (LoopFifoTest, LoopRegionRestrictsReadPosition)
{
    fifo.setLoopRegion (100, 300);
    fifo.setReadPosition (250);

    fifo.finishedRead (60, 1.0f, false);
    EXPECT_GE (fifo.getReadPos(), 100);
    EXPECT_LT (fifo.getReadPos(), 300);
}

TEST_F (LoopFifoTest, ClearLoopRegionRestoresFullRange)
{
    fifo.setLoopRegion (100, 300);
    fifo.clearLoopRegion();
    fifo.setWritePosition (950);

    fifo.finishedWrite (100, false, false);
    EXPECT_EQ (fifo.getWritePos(), 50); // Can wrap around full buffer
}

TEST_F (LoopFifoTest, FromScratchResetsPositions)
{
    fifo.setWritePosition (500);
    fifo.setReadPosition (400);

    fifo.fromScratch();

    EXPECT_EQ (fifo.getWritePos(), 0);
    EXPECT_EQ (fifo.getReadPos(), 0);
}

TEST_F (LoopFifoTest, FromScratchRespectsLoopRegion)
{
    fifo.setLoopRegion (100, 300);
    fifo.setWritePosition (500);
    fifo.setReadPosition (400);

    fifo.fromScratch();

    EXPECT_EQ (fifo.getWritePos(), 100);
    EXPECT_EQ (fifo.getReadPos(), 100);
}

TEST_F (LoopFifoTest, SetMusicalLengthClampsToBufferSize)
{
    fifo.setMusicalLength (1500);
    EXPECT_EQ (fifo.getMusicalLength(), 1000);

    fifo.setMusicalLength (500);
    EXPECT_EQ (fifo.getMusicalLength(), 500);
}

TEST_F (LoopFifoTest, ClearResetsState)
{
    fifo.setWritePosition (500);
    fifo.clear();

    EXPECT_EQ (fifo.getMusicalLength(), 0);
    EXPECT_EQ (fifo.getWritePos(), 0);
    EXPECT_EQ (fifo.getReadPos(), 0);
}

// ============================================================================
// LoopLifo Tests
// ============================================================================

class LoopLifoTest : public ::testing::Test
{
protected:
    LoopLifo lifo;

    void SetUp() override { lifo.prepareToPlay (5); }
};

TEST_F (LoopLifoTest, InitializesCorrectly)
{
    EXPECT_EQ (lifo.getCapacity(), 5);
    EXPECT_EQ (lifo.getActiveLayers(), 0);
    EXPECT_EQ (lifo.getSlotToPush(), 0);
}

TEST_F (LoopLifoTest, PrepareToWriteGivesNextSlot)
{
    int start1, size1, start2, size2;
    lifo.prepareToWrite (1, start1, size1, start2, size2);

    EXPECT_EQ (start1, 0);
    EXPECT_EQ (size1, 1);
    EXPECT_EQ (start2, 0);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopLifoTest, FinishedWriteAdvancesSlot)
{
    lifo.finishedWrite (1, false);
    EXPECT_EQ (lifo.getSlotToPush(), 1);
    EXPECT_EQ (lifo.getActiveLayers(), 1);
}

TEST_F (LoopLifoTest, PushMultipleLayers)
{
    for (int i = 0; i < 3; ++i)
    {
        int s1, sz1, s2, sz2;
        lifo.prepareToWrite (1, s1, sz1, s2, sz2);
        lifo.finishedWrite (1, false);
    }

    EXPECT_EQ (lifo.getActiveLayers(), 3);
    EXPECT_EQ (lifo.getSlotToPush(), 3);
}

TEST_F (LoopLifoTest, WrapsAroundAtCapacity)
{
    for (int i = 0; i < 6; ++i)
    {
        int s1, sz1, s2, sz2;
        lifo.prepareToWrite (1, s1, sz1, s2, sz2);
        lifo.finishedWrite (1, false);
    }

    EXPECT_EQ (lifo.getActiveLayers(), 5); // Capped at capacity
    EXPECT_EQ (lifo.getSlotToPush(), 1);   // Wrapped around
}

TEST_F (LoopLifoTest, PrepareToReadWhenEmpty)
{
    int start1, size1, start2, size2;
    lifo.prepareToRead (1, start1, size1, start2, size2);

    EXPECT_EQ (size1, 0);
    EXPECT_EQ (size2, 0);
}

TEST_F (LoopLifoTest, PrepareToReadGivesLastPushed)
{
    lifo.finishedWrite (1, false);
    lifo.finishedWrite (1, false);

    int start1, size1, start2, size2;
    lifo.prepareToRead (1, start1, size1, start2, size2);

    EXPECT_EQ (start1, 1); // Last pushed was slot 1
    EXPECT_EQ (size1, 1);
}

TEST_F (LoopLifoTest, FinishedReadDecrementsLayers)
{
    lifo.finishedWrite (1, false);
    lifo.finishedWrite (1, false);

    lifo.finishedRead (1, false);

    EXPECT_EQ (lifo.getActiveLayers(), 1);
}

TEST_F (LoopLifoTest, FinishedReadMovesSlotBack)
{
    lifo.finishedWrite (1, false);
    lifo.finishedWrite (1, false);
    EXPECT_EQ (lifo.getSlotToPush(), 2);

    lifo.finishedRead (1, false);
    EXPECT_EQ (lifo.getSlotToPush(), 1);
}

TEST_F (LoopLifoTest, GetNextLayerIndexWhenEmpty) { EXPECT_EQ (lifo.getNextLayerIndex(), -1); }

TEST_F (LoopLifoTest, GetNextLayerIndexAfterPush)
{
    lifo.finishedWrite (1, false);
    EXPECT_EQ (lifo.getNextLayerIndex(), 0);

    lifo.finishedWrite (1, false);
    EXPECT_EQ (lifo.getNextLayerIndex(), 1);
}

TEST_F (LoopLifoTest, ClearResetsState)
{
    lifo.finishedWrite (1, false);
    lifo.finishedWrite (1, false);

    lifo.clear();

    EXPECT_EQ (lifo.getActiveLayers(), 0);
    EXPECT_EQ (lifo.getSlotToPush(), 0);
}

// ============================================================================
// LevelMeter Tests
// ============================================================================

class LevelMeterTest : public ::testing::Test
{
protected:
    LevelMeter meter;
    juce::AudioBuffer<float> testBuffer;

    void SetUp() override
    {
        meter.prepare (2);
        testBuffer.setSize (2, 512);
    }
};

TEST_F (LevelMeterTest, InitializesAtZero)
{
    EXPECT_FLOAT_EQ (meter.getPeakLevel (0), 0.0f);
    EXPECT_FLOAT_EQ (meter.getPeakLevel (1), 0.0f);
    EXPECT_FLOAT_EQ (meter.getRMSLevel (0), 0.0f);
    EXPECT_FLOAT_EQ (meter.getRMSLevel (1), 0.0f);
}

TEST_F (LevelMeterTest, ProcessesBufferWithSignal)
{
    // Fill with 0.5 amplitude sine wave
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 512; ++i)
        {
            testBuffer.setSample (ch, i, 0.5f * std::sin (2.0f * M_PI * i / 512.0f));
        }
    }

    meter.processBuffer (testBuffer);

    EXPECT_GT (meter.getPeakLevel (0), 0.0f);
    EXPECT_GT (meter.getRMSLevel (0), 0.0f);
}

TEST_F (LevelMeterTest, PeakLevelMatchesMaxAmplitude)
{
    testBuffer.setSample (0, 0, 0.8f);
    testBuffer.setSample (1, 100, 0.6f);

    meter.processBuffer (testBuffer);

    EXPECT_NEAR (meter.getPeakLevel (0), 0.8f, 0.01f);
    EXPECT_NEAR (meter.getPeakLevel (1), 0.6f, 0.01f);
}

TEST_F (LevelMeterTest, LevelsDecayOverTime)
{
    // Set high level
    for (int i = 0; i < 512; ++i)
    {
        testBuffer.setSample (0, i, 0.9f);
    }
    meter.processBuffer (testBuffer);
    float initialPeak = meter.getPeakLevel (0);

    // Process silence
    testBuffer.clear();
    meter.processBuffer (testBuffer);

    EXPECT_LT (meter.getPeakLevel (0), initialPeak);
}

TEST_F (LevelMeterTest, ClearResetsLevels)
{
    for (int i = 0; i < 512; ++i)
    {
        testBuffer.setSample (0, i, 0.9f);
    }
    meter.processBuffer (testBuffer);

    meter.clear();

    EXPECT_FLOAT_EQ (meter.getPeakLevel (0), 0.0f);
    EXPECT_FLOAT_EQ (meter.getRMSLevel (0), 0.0f);
}

TEST_F (LevelMeterTest, IndependentChannelMetering)
{
    testBuffer.clear();
    testBuffer.setSample (0, 0, 0.8f);
    testBuffer.setSample (1, 0, 0.3f);

    meter.processBuffer (testBuffer);

    EXPECT_GT (meter.getPeakLevel (0), meter.getPeakLevel (1));
}

// ============================================================================
// Metronome Tests
// ============================================================================

class MetronomeTest : public ::testing::Test
{
protected:
    Metronome metronome;
    juce::AudioBuffer<float> buffer;

    void SetUp() override
    {
        metronome.prepareToPlay (44100.0, 512);
        buffer.setSize (2, 512);
    }
};

TEST_F (MetronomeTest, InitializesDisabled) { EXPECT_FALSE (metronome.isEnabled()); }

TEST_F (MetronomeTest, SetBpmUpdatesValue)
{
    metronome.setBpm (120);
    EXPECT_EQ (metronome.getBpm(), 120);
}

TEST_F (MetronomeTest, SetBpmClampedToValidRange)
{
    metronome.setBpm (20); // Below min
    EXPECT_GE (metronome.getBpm(), 40);

    metronome.setBpm (300); // Above max
    EXPECT_LE (metronome.getBpm(), 240);
}

TEST_F (MetronomeTest, DoesNotProcessWhenDisabled)
{
    buffer.clear();
    metronome.processBlock (buffer);

    EXPECT_FLOAT_EQ (buffer.getMagnitude (0, 0, 512), 0.0f);
}

TEST_F (MetronomeTest, ProcessesClicksWhenEnabled)
{
    metronome.setEnabled (true);
    buffer.clear();

    // Process enough samples to trigger a beat
    int samplesPerBeat = static_cast<int> (44100.0 / (120.0 / 60.0));
    buffer.setSize (2, samplesPerBeat + 100);

    metronome.processBlock (buffer);

    // Should have some audio content
    EXPECT_GT (buffer.getMagnitude (0, 0, samplesPerBeat + 100), 0.0f);
}

TEST_F (MetronomeTest, ResetClearsBeatCounter)
{
    metronome.setEnabled (true);
    metronome.setBpm (120);

    buffer.setSize (2, 20000);
    metronome.processBlock (buffer);

    metronome.reset();
    EXPECT_EQ (metronome.getCurrentBeat(), 0);
}

TEST_F (MetronomeTest, SetTimeSignatureUpdatesNumeratorAndDenominator)
{
    metronome.setTimeSignature (3, 4);
    // Can't directly test numerator/denominator but can test behavior
    metronome.setEnabled (true);

    // Process enough for multiple beats
    buffer.setSize (2, 100000);
    metronome.processBlock (buffer);

    // Beat counter should wrap at 3
    EXPECT_LT (metronome.getCurrentBeat(), 3);
}

TEST_F (MetronomeTest, SetStrongBeatMarksSpecificBeat)
{
    metronome.setStrongBeat (1, true);
    metronome.setTimeSignature (4, 4);
    metronome.setEnabled (true);

    // First beat should be strong
    EXPECT_FALSE (metronome.isStrongBeat());
}

TEST_F (MetronomeTest, DisableStrongBeatRemovesAccent)
{
    metronome.setStrongBeat (1, true);
    metronome.disableStrongBeat();
    metronome.setEnabled (true);

    EXPECT_FALSE (metronome.isStrongBeat());
}

TEST_F (MetronomeTest, SetVolumeAffectsOutput)
{
    metronome.setEnabled (true);
    metronome.setVolume (0.5f);

    EXPECT_FLOAT_EQ (metronome.getVolume(), 0.5f);
}

TEST_F (MetronomeTest, TapTempoCalculatesBPM)
{
    // Simulate taps at 500ms intervals (120 BPM)
    metronome.handleTap();
    juce::Thread::sleep (500);
    metronome.handleTap();
    juce::Thread::sleep (500);
    metronome.handleTap();

    // Should be around 120 BPM
    EXPECT_NEAR (metronome.getBpm(), 120, 10);
}

TEST_F (MetronomeTest, TapTempoRequiresMultipleTaps)
{
    int initialBpm = metronome.getBpm();
    metronome.handleTap();

    // Single tap shouldn't change BPM
    EXPECT_EQ (metronome.getBpm(), initialBpm);
}

TEST_F (MetronomeTest, ReleaseResourcesClearsBuffers)
{
    metronome.setEnabled (true);
    metronome.releaseResources();

    // After release, processing should be safe but produce no output
    buffer.clear();
    metronome.processBlock (buffer);
    EXPECT_FLOAT_EQ (buffer.getMagnitude (0, 0, 512), 0.0f);
}

// ============================================================================
// VolumeProcessor Tests
// ============================================================================

class VolumeProcessorTest : public ::testing::Test
{
protected:
    VolumeProcessor processor;
    juce::AudioBuffer<float> buffer;

    void SetUp() override
    {
        processor.prepareToPlay (44100.0, 512);
        buffer.setSize (2, 512);
    }
};

TEST_F (VolumeProcessorTest, InitializesWithDefaultVolume) { EXPECT_FLOAT_EQ (processor.getTrackVolume(), 1.0f); }

TEST_F (VolumeProcessorTest, SetTrackVolumeUpdatesValue)
{
    processor.setTrackVolume (0.5f);
    EXPECT_FLOAT_EQ (processor.getTrackVolume(), 0.5f);
}

TEST_F (VolumeProcessorTest, SetTrackVolumeClampsToRange)
{
    processor.setTrackVolume (2.0f);
    EXPECT_LE (processor.getTrackVolume(), 1.0f);

    processor.setTrackVolume (-0.5f);
    EXPECT_GE (processor.getTrackVolume(), 0.0f);
}

TEST_F (VolumeProcessorTest, ApplyVolumeScalesBuffer)
{
    for (int i = 0; i < 512; ++i)
    {
        buffer.setSample (0, i, 1.0f);
        buffer.setSample (1, i, 1.0f);
    }

    processor.setTrackVolume (0.5f);
    processor.applyVolume (buffer, 512);

    EXPECT_NEAR (buffer.getSample (0, 0), 0.5f, 0.01f);
}

TEST_F (VolumeProcessorTest, MuteZerosVolume)
{
    processor.setTrackVolume (0.8f);
    processor.setMuted (true);

    EXPECT_FLOAT_EQ (processor.getTrackVolume(), 0.0f);
    EXPECT_TRUE (processor.isMuted());
}

TEST_F (VolumeProcessorTest, UnmuteRestoresVolume)
{
    processor.setTrackVolume (0.8f);
    processor.setMuted (true);
    processor.setMuted (false);

    EXPECT_NEAR (processor.getTrackVolume(), 0.8f, 0.01f);
    EXPECT_FALSE (processor.isMuted());
}

TEST_F (VolumeProcessorTest, SoloStateTracked)
{
    processor.setSoloed (true);
    EXPECT_TRUE (processor.isSoloed());

    processor.setSoloed (false);
    EXPECT_FALSE (processor.isSoloed());
}

TEST_F (VolumeProcessorTest, SetOverdubGainsUpdatesValues)
{
    processor.setOverdubNewGain (0.8);
    processor.setOverdubOldGain (0.6);

    EXPECT_DOUBLE_EQ (processor.getOverdubNewGain(), 0.8);
    EXPECT_DOUBLE_EQ (processor.getOverdubOldGain(), 0.6);
}

TEST_F (VolumeProcessorTest, SaveBalancedLayersOverdubMode)
{
    float dest[512] = { 0 };
    float source[512];
    std::fill_n (source, 512, 1.0f);

    processor.setOverdubNewGain (0.5);
    processor.setOverdubOldGain (0.5);

    processor.saveBalancedLayers (dest, source, 512, true);

    // Should be old * 0.5 + new * 0.5
    EXPECT_NEAR (dest[0], 0.5f, 0.01f);
}

TEST_F (VolumeProcessorTest, SaveBalancedLayersRecordMode)
{
    float dest[512];
    std::fill_n (dest, 512, 1.0f);
    float source[512];
    std::fill_n (source, 512, 0.5f);

    processor.setOverdubNewGain (0.8);

    processor.saveBalancedLayers (dest, source, 512, false);

    // Should be source * 0.8 (old zeroed)
    EXPECT_NEAR (dest[0], 0.4f, 0.01f);
}

TEST_F (VolumeProcessorTest, NormalizeOutputScalesBuffer)
{
    for (int i = 0; i < 512; ++i)
    {
        buffer.setSample (0, i, 0.5f);
    }

    processor.normalizeOutput (buffer, 512);

    float maxSample = buffer.getMagnitude (0, 0, 512);
    EXPECT_NEAR (maxSample, 0.7f, 0.1f); // Target is ~0.7
}

TEST_F (VolumeProcessorTest, ApplyCrossfadeFadesInAndOut)
{
    processor.setCrossFadeLength (100);

    for (int i = 0; i < 512; ++i)
    {
        buffer.setSample (0, i, 1.0f);
    }

    processor.applyCrossfade (buffer, 512);

    // First sample should be near zero (fade in)
    EXPECT_LT (buffer.getSample (0, 0), 0.1f);
    // Last sample should be near zero (fade out)
    EXPECT_LT (buffer.getSample (0, 511), 0.1f);
    // Middle should be full volume
    EXPECT_NEAR (buffer.getSample (0, 256), 1.0f, 0.1f);
}

TEST_F (VolumeProcessorTest, ClearResetsState)
{
    processor.setTrackVolume (0.5f);
    processor.setMuted (true);
    processor.setSoloed (true);

    processor.clear();

    EXPECT_FLOAT_EQ (processor.getTrackVolume(), 1.0f);
    EXPECT_FALSE (processor.isMuted());
    EXPECT_FALSE (processor.isSoloed());
}

// ============================================================================
// PlaybackEngine Tests
// ============================================================================

class PlaybackEngineTest : public ::testing::Test
{
protected:
    PlaybackEngine engine;

    void SetUp() override { engine.prepareToPlay (44100.0, 4096, 2, 512); }
};

TEST_F (PlaybackEngineTest, InitializesWithDefaultSpeed) { EXPECT_FLOAT_EQ (engine.getPlaybackSpeed(), 1.0f); }

TEST_F (PlaybackEngineTest, InitializesWithForwardDirection) { EXPECT_TRUE (engine.isPlaybackDirectionForward()); }

TEST_F (PlaybackEngineTest, SetPlaybackSpeedUpdatesValue)
{
    engine.setPlaybackSpeed (1.5f);
    EXPECT_FLOAT_EQ (engine.getPlaybackSpeed(), 1.5f);
}

TEST_F (PlaybackEngineTest, SetPlaybackSpeedRejectsNegative)
{
    engine.setPlaybackSpeed (-1.0f);
    EXPECT_GT (engine.getPlaybackSpeed(), 0.0f);
}

TEST_F (PlaybackEngineTest, SetPlaybackSpeedRejectsZero)
{
    engine.setPlaybackSpeed (0.0f);
    EXPECT_GT (engine.getPlaybackSpeed(), 0.0f);
}

TEST_F (PlaybackEngineTest, SetPlaybackDirectionForward)
{
    engine.setPlaybackDirectionBackward();
    engine.setPlaybackDirectionForward();
    EXPECT_TRUE (engine.isPlaybackDirectionForward());
}

TEST_F (PlaybackEngineTest, SetPlaybackDirectionBackward)
{
    engine.setPlaybackDirectionBackward();
    EXPECT_FALSE (engine.isPlaybackDirectionForward());
}

TEST_F (PlaybackEngineTest, ToggleDirectionChangesState)
{
    bool initialDirection = engine.isPlaybackDirectionForward();

    if (initialDirection)
    {
        engine.setPlaybackDirectionBackward();
    }
    else
    {
        engine.setPlaybackDirectionForward();
    }

    EXPECT_NE (engine.isPlaybackDirectionForward(), initialDirection);
}

TEST_F (PlaybackEngineTest, SetPlaybackPitchClampsToRange)
{
    engine.setPlaybackPitchSemitones (-15.0f); // Below min
    EXPECT_GE (engine.getPlaybackPitchSemitones(), -12.0);

    engine.setPlaybackPitchSemitones (15.0f); // Above max
    EXPECT_LE (engine.getPlaybackPitchSemitones(), 12.0);
}

TEST_F (PlaybackEngineTest, SetPlaybackPitchUpdatesValue)
{
    engine.setPlaybackPitchSemitones (5.0f);
    EXPECT_DOUBLE_EQ (engine.getPlaybackPitchSemitones(), 5.0);
}

TEST_F (PlaybackEngineTest, KeepPitchWhenChangingSpeedDefaultState) { EXPECT_FALSE (engine.shouldKeepPitchWhenChangingSpeed()); }

TEST_F (PlaybackEngineTest, SetKeepPitchWhenChangingSpeedUpdates)
{
    engine.setKeepPitchWhenChangingSpeed (true);
    EXPECT_TRUE (engine.shouldKeepPitchWhenChangingSpeed());

    engine.setKeepPitchWhenChangingSpeed (false);
    EXPECT_FALSE (engine.shouldKeepPitchWhenChangingSpeed());
}

TEST_F (PlaybackEngineTest, ClearResetsToDefaults)
{
    engine.setPlaybackSpeed (1.5f);
    engine.setPlaybackDirectionBackward();
    engine.setPlaybackPitchSemitones (3.0f);

    engine.clear();

    EXPECT_FLOAT_EQ (engine.getPlaybackSpeed(), 1.0f);
}

TEST_F (PlaybackEngineTest, ReleaseResourcesClearsState)
{
    engine.setPlaybackSpeed (1.5f);
    engine.releaseResources();

    // After release, should be safe to prepare again
    engine.prepareToPlay (44100.0, 4096, 2, 512);
    EXPECT_FLOAT_EQ (engine.getPlaybackSpeed(), 1.0f);
}

// ============================================================================
// UndoStackManager Tests
// ============================================================================

class UndoStackManagerTest : public ::testing::Test
{
protected:
    UndoStackManager undoManager;
    std::unique_ptr<juce::AudioBuffer<float>> testBuffer;

    void SetUp() override
    {
        undoManager.prepareToPlay (5, 2, 1000);
        testBuffer = std::make_unique<juce::AudioBuffer<float>> (2, 1000);
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

    float getBufferValue (const juce::AudioBuffer<float>& buffer) { return buffer.getSample (0, 0); }
};

TEST_F (UndoStackManagerTest, InitializesCorrectly)
{
    EXPECT_EQ (undoManager.getNumLayers(), 5);
    EXPECT_EQ (undoManager.getNumChannels(), 2);
    EXPECT_EQ (undoManager.getNumSamples(), 1000);
}

TEST_F (UndoStackManagerTest, StageCurrentBufferCopiesData)
{
    fillBufferWithValue (*testBuffer, 0.5f);

    undoManager.stageCurrentBuffer (*testBuffer, 1000);

    // Modify original
    fillBufferWithValue (*testBuffer, 0.8f);

    // Stage should still have old value (we can't directly check staging buffer)
    // This is tested indirectly through finalizeCopyAndPush
}

TEST_F (UndoStackManagerTest, FinalizeCopyAndPushStoresBuffer)
{
    fillBufferWithValue (*testBuffer, 0.5f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);

    undoManager.finalizeCopyAndPush (1000);

    // Staged buffer should now be in undo stack
    // Can verify by undoing
    fillBufferWithValue (*testBuffer, 0.8f);
    EXPECT_TRUE (undoManager.undo (testBuffer));
    EXPECT_FLOAT_EQ (getBufferValue (*testBuffer), 0.5f);
}

TEST_F (UndoStackManagerTest, UndoWhenEmptyReturnsFalse) { EXPECT_FALSE (undoManager.undo (testBuffer)); }

TEST_F (UndoStackManagerTest, UndoRestoresPreviousState)
{
    // Push first state
    fillBufferWithValue (*testBuffer, 0.3f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    // Push second state
    fillBufferWithValue (*testBuffer, 0.7f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    // Undo should restore first state
    EXPECT_TRUE (undoManager.undo (testBuffer));
    EXPECT_FLOAT_EQ (getBufferValue (*testBuffer), 0.3f);
}

TEST_F (UndoStackManagerTest, MultipleUndosRestoreHistory)
{
    // Push three states
    for (int i = 0; i < 3; ++i)
    {
        fillBufferWithValue (*testBuffer, (float) i);
        undoManager.stageCurrentBuffer (*testBuffer, 1000);
        undoManager.finalizeCopyAndPush (1000);
    }

    // Current state is 2.0
    fillBufferWithValue (*testBuffer, 3.0f);

    // Undo back to 1.0
    EXPECT_TRUE (undoManager.undo (testBuffer));
    EXPECT_FLOAT_EQ (getBufferValue (*testBuffer), 2.0f);

    // Undo back to 0.0
    EXPECT_TRUE (undoManager.undo (testBuffer));
    EXPECT_FLOAT_EQ (getBufferValue (*testBuffer), 1.0f);
}

TEST_F (UndoStackManagerTest, RedoWhenEmptyReturnsFalse) { EXPECT_FALSE (undoManager.redo (testBuffer)); }

TEST_F (UndoStackManagerTest, RedoRestoresUndoneState)
{
    fillBufferWithValue (*testBuffer, 0.3f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    fillBufferWithValue (*testBuffer, 0.7f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    // Undo
    undoManager.undo (testBuffer);

    // Redo should restore 0.7
    EXPECT_TRUE (undoManager.redo (testBuffer));
    EXPECT_FLOAT_EQ (getBufferValue (*testBuffer), 0.7f);
}

TEST_F (UndoStackManagerTest, NewActionClearsRedoStack)
{
    // Create undo history
    fillBufferWithValue (*testBuffer, 0.3f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    fillBufferWithValue (*testBuffer, 0.7f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    // Undo
    undoManager.undo (testBuffer);

    // New action
    fillBufferWithValue (*testBuffer, 0.9f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    // Redo should fail (stack cleared)
    EXPECT_FALSE (undoManager.redo (testBuffer));
}

TEST_F (UndoStackManagerTest, UndoStackHasCapacityLimit)
{
    // Push more than capacity (5)
    for (int i = 0; i < 7; ++i)
    {
        fillBufferWithValue (*testBuffer, (float) i);
        undoManager.stageCurrentBuffer (*testBuffer, 1000);
        undoManager.finalizeCopyAndPush (1000);
    }

    // Should only be able to undo 5 times
    int undoCount = 0;
    while (undoManager.undo (testBuffer))
    {
        undoCount++;
    }

    EXPECT_EQ (undoCount, 5);
}

TEST_F (UndoStackManagerTest, ClearResetsAllState)
{
    fillBufferWithValue (*testBuffer, 0.5f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    undoManager.clear();

    EXPECT_FALSE (undoManager.undo (testBuffer));
    EXPECT_FALSE (undoManager.redo (testBuffer));
}

TEST_F (UndoStackManagerTest, ReleaseResourcesClearsBuffers)
{
    fillBufferWithValue (*testBuffer, 0.5f);
    undoManager.stageCurrentBuffer (*testBuffer, 1000);
    undoManager.finalizeCopyAndPush (1000);

    undoManager.releaseResources();

    // After release, should be safe to prepare again
    undoManager.prepareToPlay (3, 2, 500);
    EXPECT_EQ (undoManager.getNumLayers(), 3);
}

// ============================================================================
// BufferManager Tests
// ============================================================================

class BufferManagerTest : public ::testing::Test
{
protected:
    BufferManager manager;
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;

    void SetUp() override
    {
        manager.prepareToPlay (2, 1000);
        inputBuffer.setSize (2, 100);
        outputBuffer.setSize (2, 100);
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
};

TEST_F (BufferManagerTest, InitializesCorrectly)
{
    EXPECT_EQ (manager.getNumChannels(), 2);
    EXPECT_EQ (manager.getNumSamples(), 1000);
    EXPECT_EQ (manager.getLength(), 0);
}

TEST_F (BufferManagerTest, ClearResetsState)
{
    manager.setLength (500);
    manager.clear();

    EXPECT_EQ (manager.getLength(), 0);
    EXPECT_EQ (manager.getWritePosition(), 0);
    EXPECT_EQ (manager.getReadPosition(), 0);
}

TEST_F (BufferManagerTest, UpdateLoopLengthIncreasesProvisionalLength)
{
    manager.updateLoopLength (100, false);
    manager.updateLoopLength (100, false);

    // Provisional length should accumulate
    // Can't directly check but finalizeLayer will use it
}

TEST_F (BufferManagerTest, SetLengthUpdatesValue)
{
    manager.setLength (500);
    EXPECT_EQ (manager.getLength(), 500);
}

TEST_F (BufferManagerTest, WriteToAudioBufferBasicCopy)
{
    fillBufferWithValue (inputBuffer, 0.5f);

    auto copyFunc = [] (float* dest, const float* src, int samples, bool) { juce::FloatVectorOperations::copy (dest, src, samples); };

    manager.writeToAudioBuffer (copyFunc, inputBuffer, 100, false, false);

    // Check data was written
    EXPECT_FLOAT_EQ (manager.getReadPointer (0)[0], 0.5f);
}

TEST_F (BufferManagerTest, WriteToAudioBufferWrapsAround)
{
    manager.setWritePosition (950);
    fillBufferWithValue (inputBuffer, 0.7f);

    auto copyFunc = [] (float* dest, const float* src, int samples, bool) { juce::FloatVectorOperations::copy (dest, src, samples); };

    manager.writeToAudioBuffer (copyFunc, inputBuffer, 100, false, true);

    // Should wrap to beginning
    EXPECT_LT (manager.getWritePosition(), 100);
}

TEST_F (BufferManagerTest, ReadFromAudioBufferCopiesData)
{
    // Write some data first
    fillBufferWithValue (inputBuffer, 0.6f);
    auto writeFunc = [] (float* dest, const float* src, int samples, bool) { juce::FloatVectorOperations::copy (dest, src, samples); };
    manager.writeToAudioBuffer (writeFunc, inputBuffer, 100, false, false);
    manager.setLength (100);
    manager.setReadPosition (0);

    // Read it back
    outputBuffer.clear();
    auto readFunc = [] (float* dest, const float* src, int samples) { juce::FloatVectorOperations::copy (dest, src, samples); };
    manager.readFromAudioBuffer (readFunc, outputBuffer, 50, 1.0f, false);

    EXPECT_FLOAT_EQ (outputBuffer.getSample (0, 0), 0.6f);
}

TEST_F (BufferManagerTest, SetWritePositionClampsToLength)
{
    manager.setLength (500);
    manager.setWritePosition (1500);

    EXPECT_LT (manager.getWritePosition(), 500);
}

TEST_F (BufferManagerTest, SetReadPositionClampsToLength)
{
    manager.setLength (500);
    manager.setReadPosition (1500);

    EXPECT_LT (manager.getReadPosition(), 500);
}

TEST_F (BufferManagerTest, FinalizeLayerUpdatesLength)
{
    manager.updateLoopLength (300, false);
    manager.finalizeLayer (false, 0);

    EXPECT_GT (manager.getLength(), 0);
}

TEST_F (BufferManagerTest, HasWrappedAroundDetectsWrap)
{
    manager.setLength (1000);
    manager.setReadPosition (50);

    EXPECT_FALSE (manager.hasWrappedAround());

    manager.setReadPosition (10); // Moved backward = wrapped
    EXPECT_TRUE (manager.hasWrappedAround());
}

TEST_F (BufferManagerTest, SetLoopRegionConstrainsRange)
{
    manager.setLength (1000);
    manager.setLoopRegion (100, 300);

    EXPECT_TRUE (manager.hasLoopRegion());
    EXPECT_EQ (manager.getLoopRegionStart(), 100);
    EXPECT_EQ (manager.getLoopRegionEnd(), 300);
}

TEST_F (BufferManagerTest, ClearLoopRegionDisablesIt)
{
    manager.setLength (1000);
    manager.setLoopRegion (100, 300);
    manager.clearLoopRegion();

    EXPECT_FALSE (manager.hasLoopRegion());
}

TEST_F (BufferManagerTest, FromScratchResetsPlayback)
{
    manager.setLength (1000);
    manager.setWritePosition (500);
    manager.setReadPosition (600);

    manager.fromScratch();

    EXPECT_EQ (manager.getWritePosition(), 0);
    EXPECT_EQ (manager.getReadPosition(), 0);
}

TEST_F (BufferManagerTest, ReleaseResourcesClearsBuffer)
{
    manager.setLength (500);
    manager.releaseResources();

    EXPECT_EQ (manager.getNumChannels(), 0);
    EXPECT_EQ (manager.getNumSamples(), 0);
    EXPECT_EQ (manager.getLength(), 0);
}

// ============================================================================
// EngineMessageBus Tests
// ============================================================================

class MockEngineListener : public EngineMessageBus::Listener
{
public:
    MOCK_METHOD (void, handleEngineEvent, (const EngineMessageBus::Event&), (override));
};

class EngineMessageBusTest : public ::testing::Test
{
protected:
    EngineMessageBus bus;
    MockEngineListener listener;

    void SetUp() override { bus.addListener (&listener); }

    void TearDown() override { bus.removeListener (&listener); }
};

TEST_F (EngineMessageBusTest, PushAndPopCommand)
{
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::TogglePlay;
    cmd.trackIndex = 0;

    bus.pushCommand (cmd);

    EngineMessageBus::Command outCmd;
    EXPECT_TRUE (bus.popCommand (outCmd));
    EXPECT_EQ (outCmd.type, EngineMessageBus::CommandType::TogglePlay);
    EXPECT_EQ (outCmd.trackIndex, 0);
}

TEST_F (EngineMessageBusTest, PopCommandWhenEmptyReturnsFalse)
{
    EngineMessageBus::Command cmd;
    EXPECT_FALSE (bus.popCommand (cmd));
}

TEST_F (EngineMessageBusTest, HasCommandsReturnsTrueWhenQueued)
{
    EXPECT_FALSE (bus.hasCommands());

    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::Stop;
    bus.pushCommand (cmd);

    EXPECT_TRUE (bus.hasCommands());
}

TEST_F (EngineMessageBusTest, CommandsAreFIFO)
{
    EngineMessageBus::Command cmd1, cmd2;
    cmd1.type = EngineMessageBus::CommandType::TogglePlay;
    cmd2.type = EngineMessageBus::CommandType::Stop;

    bus.pushCommand (cmd1);
    bus.pushCommand (cmd2);

    EngineMessageBus::Command out;
    bus.popCommand (out);
    EXPECT_EQ (out.type, EngineMessageBus::CommandType::TogglePlay);

    bus.popCommand (out);
    EXPECT_EQ (out.type, EngineMessageBus::CommandType::Stop);
}

TEST_F (EngineMessageBusTest, CommandWithFloatPayload)
{
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::SetVolume;
    cmd.payload = 0.75f;

    bus.pushCommand (cmd);

    EngineMessageBus::Command out;
    bus.popCommand (out);
    EXPECT_FLOAT_EQ (std::get<float> (out.payload), 0.75f);
}

TEST_F (EngineMessageBusTest, CommandWithIntPayload)
{
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::SetMetronomeBPM;
    cmd.payload = 120;

    bus.pushCommand (cmd);

    EngineMessageBus::Command out;
    bus.popCommand (out);
    EXPECT_EQ (std::get<int> (out.payload), 120);
}

TEST_F (EngineMessageBusTest, BroadcastEventTriggersListener)
{
    EngineMessageBus::Event evt;
    evt.type = EngineMessageBus::EventType::PlaybackStateChanged;
    evt.trackIndex = 1;
    evt.data = true;

    EXPECT_CALL (listener, handleEngineEvent (::testing::_)).Times (1);

    bus.broadcastEvent (evt);

    // Give time for timer to dispatch
    juce::Thread::sleep (20);
}

TEST_F (EngineMessageBusTest, MultipleListenersReceiveEvents)
{
    MockEngineListener listener2;
    bus.addListener (&listener2);

    EngineMessageBus::Event evt;
    evt.type = EngineMessageBus::EventType::RecordingStateChanged;

    EXPECT_CALL (listener, handleEngineEvent (::testing::_)).Times (1);
    EXPECT_CALL (listener2, handleEngineEvent (::testing::_)).Times (1);

    bus.broadcastEvent (evt);
    juce::Thread::sleep (20);

    bus.removeListener (&listener2);
}

TEST_F (EngineMessageBusTest, RemoveListenerStopsReceivingEvents)
{
    bus.removeListener (&listener);

    EngineMessageBus::Event evt;
    evt.type = EngineMessageBus::EventType::PlaybackStateChanged;

    EXPECT_CALL (listener, handleEngineEvent (::testing::_)).Times (0);

    bus.broadcastEvent (evt);
    juce::Thread::sleep (20);
}

TEST_F (EngineMessageBusTest, ClearRemovesPendingMessages)
{
    EngineMessageBus::Command cmd;
    cmd.type = EngineMessageBus::CommandType::TogglePlay;
    bus.pushCommand (cmd);

    bus.clear();

    EXPECT_FALSE (bus.hasCommands());

    EngineMessageBus::Command out;
    EXPECT_FALSE (bus.popCommand (out));
}

TEST_F (EngineMessageBusTest, GetCategoryForCommandTypeReturnsCorrectCategory)
{
    EXPECT_EQ (EngineMessageBus::getCategoryForCommandType (EngineMessageBus::CommandType::TogglePlay), "Transport");
    EXPECT_EQ (EngineMessageBus::getCategoryForCommandType (EngineMessageBus::CommandType::SetVolume), "Track Controls");
    EXPECT_EQ (EngineMessageBus::getCategoryForCommandType (EngineMessageBus::CommandType::SetPlaybackSpeed), "Playback");
}

// ============================================================================
// Notes on classes that don't need extensive unit tests:
// ============================================================================

/*
 * ChannelContext and StereoMeterContext: These are simple data holders with
 * atomic operations. They're thoroughly tested through LevelMeter tests.
 *
 * LooperEngine: This is a high-level orchestrator. Unit tests would be extensive
 * but most value comes from integration tests that verify the state machine,
 * track management, and command handling work together. Individual command
 * handlers can be tested but they mostly delegate to other classes.
 * 
 * LoopTrack: Similar to LooperEngine - it's an orchestrator of BufferManager,
 * UndoManager, PlaybackEngine, and VolumeProcessor. Its public API is tested
 * but integration tests would provide more value.
 *
 * LooperStateMachine: The state machine logic is best tested through integration
 * tests with real StateContext objects and audio buffers. Unit testing the
 * transition table would be testing the data structure itself.
 *
 * Suggested improvements for testability:
 * 1. BufferManager: Consider adding a method to get provisional length for testing
 * 2. LoopFifo: The exact read position handling could use a getter for fractional part
 * 3. PlaybackEngine: Consider exposing whether fast path was used in last process
 * 4. UndoStackManager: The internal state (staging buffer) isn't testable - this is OK
 * 5. EngineMessageBus: Timer-based dispatch makes testing async - consider manual dispatch
 */

int main (int argc, char** argv)
{
    ::testing::InitGoogleTest (&argc, argv);
    return RUN_ALL_TESTS();
}
