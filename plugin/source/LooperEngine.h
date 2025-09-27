#pragma once

#include "LoopTrack.h"
#include <JuceHeader.h>

enum class TransportState
{
    Stopped,
    Recording,
    Overdubbing,
    Playing
};

class LooperEngine
{
public:
    LooperEngine();
    ~LooperEngine();

    void prepareToPlay (double sampleRate, int maxBlockSize, int numTracks, int numChannels);
    void releaseResources();

    void addTrack();
    void selectTrack (const int trackIndex);
    void removeTrack (const int trackIndex);
    LoopTrack* getActiveTrack();
    int getActiveTrackIndex() const
    {
        return activeTrackIndex;
    }
    int getNumTracks() const
    {
        return numTracks;
    }

    void processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    void setOverdubGainsForTrack (const int trackIndex, const double oldGain, const double newGain);

    TransportState getTransportState() const
    {
        return transportState;
    }

    void startRecording();
    void startOverdubbing();
    void startPlaying();
    void stop();
    void undo();
    void clear();

private:
    struct MidiKey
    {
        int noteNumber;
        bool isNoteOn;

        bool operator== (const MidiKey& other) const
        {
            return noteNumber == other.noteNumber && isNoteOn == other.isNoteOn;
        }
    };

    struct MidiKeyHash
    {
        std::size_t operator() (const MidiKey& k) const
        {
            return std::hash<int>() (k.noteNumber) ^ std::hash<bool>() (k.isNoteOn);
        }
    };

    bool isRecording() const
    {
        return transportState == TransportState::Recording;
    }
    bool isOverdubbing() const
    {
        return transportState == TransportState::Overdubbing;
    }
    bool isPlaying() const
    {
        return transportState == TransportState::Playing;
    }
    bool isStopped() const
    {
        return transportState == TransportState::Stopped;
    }

    std::unordered_map<MidiKey, std::function<void (LooperEngine&)>, MidiKeyHash> midiCommandMap;
    void setupMidiCommands();
    void handleMidiCommand (const juce::MidiBuffer& midiMessages);

    TransportState transportState;
    double sampleRate;
    int maxBlockSize;
    int numChannels;
    int numTracks;
    int activeTrackIndex { 0 };

    std::vector<std::unique_ptr<LoopTrack>> loopTracks;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEngine)
};
