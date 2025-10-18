#pragma once

#include "audio/AudioToUIBridge.h"
#include "engine/LoopTrack.h"
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
    void selectNextTrack()
    {
        PERFETTO_FUNCTION();
        selectTrack ((activeTrackIndex + 1) % numTracks);
    }

    void selectPreviousTrack()
    {
        PERFETTO_FUNCTION();
        selectTrack ((activeTrackIndex - 1 + numTracks) % numTracks);
    }

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
    void undo (size_t trackIndex);
    void redo (size_t trackIndex);
    void clear (size_t trackIndex);

    void loadBackingTrackToTrack (const juce::AudioBuffer<float>& backingTrack, size_t trackIndex);
    void loadWaveFileToTrack (const juce::File& audioFile, size_t trackIndex);

    AudioToUIBridge* getUIBridgeByIndex (int trackIndex)
    {
        PERFETTO_FUNCTION();
        if (trackIndex >= 0 && trackIndex < (int) uiBridges.size()) return uiBridges[trackIndex].get();
        return nullptr;
    }

    LoopTrack* getTrackByIndex (int trackIndex)
    {
        PERFETTO_FUNCTION();
        if (trackIndex >= 0 && trackIndex < (int) loopTracks.size()) return loopTracks[trackIndex].get();
        return nullptr;
    }

public:
    void setTrackPlaybackSpeed (int trackIndex, float speed)
    {
        PERFETTO_FUNCTION();
        if (trackIndex < 0 || trackIndex >= numTracks) return;

        auto& track = loopTracks[trackIndex];
        if (track) track->setPlaybackSpeed (speed);
    }

    void setTrackPlaybackDirectionForward (int trackIndex)
    {
        PERFETTO_FUNCTION();
        if (trackIndex < 0 || trackIndex >= numTracks) return;

        auto& track = loopTracks[trackIndex];
        if (track) track->setPlaybackDirectionForward();
    }

    void setTrackPlaybackDirectionBackward (int trackIndex)
    {
        PERFETTO_FUNCTION();
        if (trackIndex < 0 || trackIndex >= numTracks) return;

        auto& track = loopTracks[trackIndex];
        if (track) track->setPlaybackDirectionBackward();
    }

    float getTrackPlaybackSpeed (int trackIndex) const
    {
        PERFETTO_FUNCTION();
        if (trackIndex < 0 || trackIndex >= numTracks) return 1.0f;

        auto& track = loopTracks[trackIndex];
        if (track) return track->getPlaybackSpeed();
        return 1.0f;
    }

    bool isTrackPlaybackForward (int trackIndex) const
    {
        PERFETTO_FUNCTION();
        if (trackIndex < 0 || trackIndex >= numTracks) return true;

        auto& track = loopTracks[trackIndex];
        if (track) return track->isPlaybackDirectionForward();
        return true;
    }

    void setTrackVolume (int trackIndex, float volume);
    void setTrackMuted (int trackIndex, bool muted);
    void setTrackSoloed (int trackIndex, bool soloed);
    float getTrackVolume (int trackIndex) const;
    bool isTrackMuted (int trackIndex) const;
    void handleMidiCommand (const juce::MidiBuffer& midiMessages);

private:
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

    bool isRecording() const
    {
        PERFETTO_FUNCTION();
        return transportState == TransportState::Recording;
    }
    bool isPlaying() const
    {
        PERFETTO_FUNCTION();
        return transportState == TransportState::Playing;
    }
    bool isStopped() const
    {
        PERFETTO_FUNCTION();
        return transportState == TransportState::Stopped;
    }

    std::unordered_map<MidiKey, std::function<void (LooperEngine&, int)>, MidiKeyHash> midiCommandMap;
    void setupMidiCommands();

    TransportState transportState;
    double sampleRate;
    int maxBlockSize;
    int numChannels;
    int numTracks;
    int activeTrackIndex { 0 };
    int nextTrackIndex { -1 };

    std::vector<std::unique_ptr<LoopTrack>> loopTracks;
    std::vector<std::unique_ptr<AudioToUIBridge>> uiBridges;
    std::vector<bool> bridgeInitialized;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEngine)
};
