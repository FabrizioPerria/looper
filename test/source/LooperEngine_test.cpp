#include <gtest/gtest.h>

#include "engine/LooperEngine.h"

class LooperEngineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        engine.prepareToPlay (8000.0, 64, 2, 2); // reduced sample rate and buffer size
    }

    LooperEngine engine;
};

TEST_F (LooperEngineTest, InitialState)
{
    EXPECT_EQ (engine.getState(), LooperState::Idle);
    EXPECT_EQ (engine.getNumTracks(), 2);
    EXPECT_EQ (engine.getActiveTrackIndex(), 1);
}

TEST_F (LooperEngineTest, TransportStateTransitions)
{
    engine.toggleRecord();
    EXPECT_EQ (engine.getState(), LooperState::Recording);

    engine.toggleRecord();
    EXPECT_EQ (engine.getState(), LooperState::Playing);

    engine.stop();
    EXPECT_EQ (engine.getState(), LooperState::Stopped);
}

TEST_F (LooperEngineTest, TrackManagement)
{
    engine.addTrack();
    EXPECT_EQ (engine.getNumTracks(), 3);
    EXPECT_EQ (engine.getActiveTrackIndex(), 2);

    engine.removeTrack (1);
    EXPECT_EQ (engine.getNumTracks(), 2);

    engine.selectTrack (0);
    EXPECT_EQ (engine.getActiveTrackIndex(), 0);
}

TEST_F (LooperEngineTest, MidiCommandHandling)
{
    juce::AudioBuffer<float> audio (2, 64);
    juce::MidiBuffer midi;
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn (1, 60, 1.0f);
    midi.addEvent (noteOn, 0);

    engine.processBlock (audio, midi);
    EXPECT_EQ (engine.getState(), LooperState::Recording);
}

TEST_F (LooperEngineTest, AudioProcessing)
{
    juce::AudioBuffer<float> buffer (2, 64);
    juce::MidiBuffer midiMessages;

    // Fill buffer with test data
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* data = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            data[sample] = 0.5f; // Test tone
        }
    }

    // Test recording
    engine.record();
    engine.processBlock (buffer, midiMessages);

    // Test playback
    engine.stop();

    juce::AudioBuffer<float> outputBuffer (2, 64);
    engine.processBlock (outputBuffer, midiMessages);

    // Verify output isn't silent
    bool hasOutput = false;
    for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
    {
        const float* data = outputBuffer.getReadPointer (channel);
        for (int sample = 0; sample < outputBuffer.getNumSamples(); ++sample)
        {
            if (std::abs (data[sample]) > 0.0f)
            {
                hasOutput = true;
                break;
            }
        }
    }
    EXPECT_TRUE (hasOutput);
}

TEST_F (LooperEngineTest, UndoAndClear)
{
    juce::AudioBuffer<float> buffer (2, 64);
    juce::MidiBuffer midiMessages;

    // Record something
    engine.record();
    engine.processBlock (buffer, midiMessages);
    engine.stop();
    EXPECT_EQ (engine.getState(), LooperState::Playing);

    // Test undo
    engine.undo (-1);
    EXPECT_EQ (engine.getState(), LooperState::Playing);

    // Test clear
    engine.clear (-1);
    EXPECT_EQ (engine.getState(), LooperState::Stopped);
}

TEST_F (LooperEngineTest, VolumeControl)
{
    engine.setTrackVolume (0, 0.5f);
    EXPECT_FLOAT_EQ (engine.getTrackVolume (0), 0.5f);

    engine.setTrackVolume (0, 0.0f);
    EXPECT_FLOAT_EQ (engine.getTrackVolume (0), 0.0f);

    engine.setTrackVolume (0, 1.0f);
    EXPECT_FLOAT_EQ (engine.getTrackVolume (0), 1.0f);
}

TEST_F (LooperEngineTest, MuteUnmute)
{
    EXPECT_FALSE (engine.isTrackMuted (0));

    engine.setTrackMuted (0, true);
    EXPECT_TRUE (engine.isTrackMuted (0));

    engine.setTrackMuted (0, false);
    EXPECT_FALSE (engine.isTrackMuted (0));
}

TEST_F (LooperEngineTest, SoloTrack)
{
    // Solo track 0, should mute track 1
    engine.setTrackSoloed (0, true);

    auto* track0 = engine.getTrackByIndex (0);
    auto* track1 = engine.getTrackByIndex (1);

    EXPECT_TRUE (track0->isSoloed());
    EXPECT_FALSE (track0->isMuted());
    EXPECT_TRUE (track1->isMuted());

    // Unsolo
    engine.setTrackSoloed (0, false);
    EXPECT_FALSE (track0->isSoloed());
    EXPECT_FALSE (track1->isMuted());
}

TEST_F (LooperEngineTest, PlaybackSpeedControl)
{
    engine.setTrackPlaybackSpeed (0, 0.5f);
    EXPECT_FLOAT_EQ (engine.getTrackPlaybackSpeed (0), 0.5f);

    engine.setTrackPlaybackSpeed (0, 2.0f);
    EXPECT_FLOAT_EQ (engine.getTrackPlaybackSpeed (0), 2.0f);
}

TEST_F (LooperEngineTest, PlaybackDirectionControl)
{
    EXPECT_TRUE (engine.isTrackPlaybackForward (0));

    engine.setTrackPlaybackDirectionBackward (0);
    EXPECT_FALSE (engine.isTrackPlaybackForward (0));

    engine.setTrackPlaybackDirectionForward (0);
    EXPECT_TRUE (engine.isTrackPlaybackForward (0));
}

TEST_F (LooperEngineTest, KeepPitchWhenChangingSpeed)
{
    EXPECT_FALSE (engine.getKeepPitchWhenChangingSpeed (0));

    engine.setKeepPitchWhenChangingSpeed (0, true);
    EXPECT_TRUE (engine.getKeepPitchWhenChangingSpeed (0));

    engine.setKeepPitchWhenChangingSpeed (0, false);
    EXPECT_FALSE (engine.getKeepPitchWhenChangingSpeed (0));
}

TEST_F (LooperEngineTest, NextAndPreviousTrack)
{
    engine.selectTrack (0);
    EXPECT_EQ (engine.getActiveTrackIndex(), 0);

    engine.selectNextTrack();
    EXPECT_EQ (engine.getActiveTrackIndex(), 1);

    engine.selectNextTrack();
    EXPECT_EQ (engine.getActiveTrackIndex(), 0); // Wraps around

    engine.selectPreviousTrack();
    EXPECT_EQ (engine.getActiveTrackIndex(), 1); // Wraps around
}

TEST_F (LooperEngineTest, OverdubGains)
{
    // Just verify it doesn't crash
    engine.setOverdubGainsForTrack (0, 0.7, 1.0);
    engine.setOverdubGainsForTrack (1, 0.5, 0.8);
}

TEST_F (LooperEngineTest, MidiVolumeCC)
{
    juce::AudioBuffer<float> audio (2, 64);
    juce::MidiBuffer midi;

    // Track volume CC (CC 7)
    juce::MidiMessage volumeCC = juce::MidiMessage::controllerEvent (1, 7, 64);
    midi.addEvent (volumeCC, 0);

    engine.processBlock (audio, midi);

    // Volume should be approximately 0.5 (64/127)
    float volume = engine.getTrackVolume (engine.getActiveTrackIndex());
    EXPECT_NEAR (volume, 0.5f, 0.01f);
}

TEST_F (LooperEngineTest, MidiSpeedCC)
{
    juce::AudioBuffer<float> audio (2, 64);
    juce::MidiBuffer midi;

    // Playback speed CC (CC 1) - value 64 should give speed around 1.1
    juce::MidiMessage speedCC = juce::MidiMessage::controllerEvent (1, 1, 64);
    midi.addEvent (speedCC, 0);

    engine.processBlock (audio, midi);

    float speed = engine.getTrackPlaybackSpeed (engine.getActiveTrackIndex());
    EXPECT_GT (speed, 0.2f);
    EXPECT_LT (speed, 2.0f);
}

TEST_F (LooperEngineTest, MidiTrackSelectCC)
{
    juce::AudioBuffer<float> audio (2, 64);
    juce::MidiBuffer midi;

    // Track select CC (CC 102) - value 1
    juce::MidiMessage trackSelectCC = juce::MidiMessage::controllerEvent (1, 102, 1);
    midi.addEvent (trackSelectCC, 0);

    engine.processBlock (audio, midi);

    // Should have signaled a track change
    EXPECT_EQ (engine.trackBeingChanged(), 1);
}

TEST_F (LooperEngineTest, InvalidTrackIndex)
{
    // Should not crash with invalid indices
    engine.setTrackVolume (-1, 0.5f);
    engine.setTrackVolume (100, 0.5f);

    EXPECT_FLOAT_EQ (engine.getTrackVolume (-1), 1.0f);
    EXPECT_FLOAT_EQ (engine.getTrackVolume (100), 1.0f);
}

TEST_F (LooperEngineTest, ReleaseResources)
{
    engine.releaseResources();

    EXPECT_EQ (engine.getNumTracks(), 0);
    EXPECT_EQ (engine.getActiveTrackIndex(), 0);
    EXPECT_EQ (engine.getState(), LooperState::Idle);
}

TEST_F (LooperEngineTest, UIBridgeAccess)
{
    auto* bridge = engine.getUIBridgeByIndex (0);
    EXPECT_NE (bridge, nullptr);

    auto* invalidBridge = engine.getUIBridgeByIndex (100);
    EXPECT_EQ (invalidBridge, nullptr);
}

TEST_F (LooperEngineTest, TrackAccess)
{
    auto* track = engine.getTrackByIndex (0);
    EXPECT_NE (track, nullptr);

    auto* activeTrack = engine.getActiveTrack();
    EXPECT_NE (activeTrack, nullptr);

    auto* invalidTrack = engine.getTrackByIndex (100);
    EXPECT_EQ (invalidTrack, nullptr);
}
