#pragma once

#include "AudioToUIBridge.h"
#include "LoopTrack.h"
#include <JuceHeader.h>

enum class TransportState
{
    Stopped,
    Recording,
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
        PERFETTO_FUNCTION();
        return activeTrackIndex;
    }
    int getNumTracks() const
    {
        PERFETTO_FUNCTION();
        return numTracks;
    }

    void processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    void setOverdubGainsForTrack (const int trackIndex, const double oldGain, const double newGain);

    TransportState getTransportState() const
    {
        PERFETTO_FUNCTION();
        return transportState;
    }

    void startRecording();
    void startPlaying();
    void stop();
    void undo();
    void redo();
    void clear();

    void loadBackingTrackToActiveTrack (const juce::AudioBuffer<float>& backingTrack);

    void loadWaveFileToActiveTrack (const juce::File& audioFile)
    {
        PERFETTO_FUNCTION();
        auto activeTrack = getActiveTrack();
        if (activeTrack)
        {
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
            if (reader)
            {
                juce::AudioBuffer<float> backingTrack ((int) reader->numChannels, (int) reader->lengthInSamples);
                reader->read (&backingTrack, 0, (int) reader->lengthInSamples, 0, true, true);
                loadBackingTrackToActiveTrack (backingTrack);
            }
        }
    }

    void setUIBridge (AudioToUIBridge* bridge)
    {
        uiBridge = bridge;
        bridgeInitialized = false; // Force initial snapshot on next processBlock
    }

    AudioToUIBridge* getUIBridge()
    {
        return uiBridge;
    }

private:
    AudioToUIBridge* uiBridge = nullptr;
    bool waveformDirty = false;
    int recordingUpdateCounter = 0;
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

    bool bridgeInitialized = false;

    std::vector<std::unique_ptr<LoopTrack>> loopTracks;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEngine)
};
