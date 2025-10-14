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
    EXPECT_EQ (engine.getTransportState(), TransportState::Stopped);
    EXPECT_EQ (engine.getNumTracks(), 2);
    EXPECT_EQ (engine.getActiveTrackIndex(), 1);
}

TEST_F (LooperEngineTest, TransportStateTransitions)
{
    engine.startRecording();
    EXPECT_EQ (engine.getTransportState(), TransportState::Recording);

    engine.stop();
    EXPECT_EQ (engine.getTransportState(), TransportState::Playing);

    engine.stop();
    EXPECT_EQ (engine.getTransportState(), TransportState::Stopped);
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
    EXPECT_EQ (engine.getTransportState(), TransportState::Recording);
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
    engine.startRecording();
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
    engine.startRecording();
    engine.processBlock (buffer, midiMessages);
    engine.stop();

    // Test undo
    engine.undo (-1);

    // Test clear
    engine.clear (-1);
    EXPECT_EQ (engine.getTransportState(), TransportState::Stopped);
}
