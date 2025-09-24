#pragma once

#include "LoopTrack.h"
#include <JuceHeader.h>

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

    void processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

private:
    enum class TransportState
    {
        Stopped,
        Recording,
        Overdubbing,
        Playing
    };

    struct MidiKey
    {
        int noteNumber;
        bool isNoteOn;

        bool operator== (const MidiKey& other) const { return noteNumber == other.noteNumber && isNoteOn == other.isNoteOn; }
    };

    struct MidiKeyHash
    {
        std::size_t operator() (const MidiKey& k) const { return std::hash<int>() (k.noteNumber) ^ std::hash<bool>() (k.isNoteOn); }
    };

    bool isRecording() const { return transportState == TransportState::Recording; }
    bool isOverdubbing() const { return transportState == TransportState::Overdubbing; }
    bool isPlaying() const { return transportState == TransportState::Playing; }
    bool isStopped() const { return transportState == TransportState::Stopped; }

    void startRecording();
    void startOverdubbing();
    void startPlaying();
    void stop();
    void undo();
    void clear();

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
