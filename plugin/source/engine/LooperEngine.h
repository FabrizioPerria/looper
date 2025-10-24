#pragma once

#include "audio/AudioToUIBridge.h"
#include "engine/LoopTrack.h"
#include <JuceHeader.h>
#include <cstddef>

enum class LooperState
{
    Idle,               // No tracks have content
    Stopped,            // Tracks exist but not playing
    Playing,            // Playing back loops
    Recording,          // Recording on empty track
    Overdubbing,        // Recording on track with content
    PendingTrackChange, // Waiting to switch tracks
    Transitioning       // Actively switching
};

struct PendingAction
{
    enum class Type
    {
        None,
        SwitchTrack,
        FinalizeRecording,
        CancelRecording
    };

    Type type = Type::None;
    int targetTrackIndex = -1;
    bool waitForWrapAround = false;

    void clear()
    {
        type = Type::None;
        targetTrackIndex = -1;
        waitForWrapAround = false;
    }

    bool isActive() const { return type != Type::None; }
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

    int trackBeingChanged() const
    {
        if (nextTrackIndex >= 0) return nextTrackIndex;
        return activeTrackIndex;
    }

    LoopTrack* getActiveTrack();

    void selectNextTrack() { selectTrack ((activeTrackIndex + 1) % numTracks); }
    void selectPreviousTrack() { selectTrack ((activeTrackIndex - 1 + numTracks) % numTracks); }

    int getActiveTrackIndex() const { return activeTrackIndex; }
    int getNumTracks() const { return numTracks; }

    void processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    void setOverdubGainsForTrack (const int trackIndex, const double oldGain, const double newGain);

    void loadBackingTrackToTrack (const juce::AudioBuffer<float>& backingTrack, int trackIndex);
    void loadWaveFileToTrack (const juce::File& audioFile, int trackIndex);

    AudioToUIBridge* getUIBridgeByIndex (int trackIndex)
    {
        if (trackIndex >= 0 && trackIndex < (int) uiBridges.size()) return uiBridges[(size_t) trackIndex].get();
        return nullptr;
    }

    LoopTrack* getTrackByIndex (int trackIndex)
    {
        if (trackIndex >= 0 && trackIndex < (int) loopTracks.size()) return loopTracks[(size_t) trackIndex].get();
        return nullptr;
    }

    void setTrackPlaybackSpeed (int trackIndex, float speed)
    {
        if (trackIndex < 0 || trackIndex >= numTracks) return;

        auto& track = loopTracks[(size_t) trackIndex];
        if (track) track->setPlaybackSpeed (speed);
    }

    void setTrackPlaybackDirectionForward (int trackIndex)
    {
        if (trackIndex < 0 || trackIndex >= numTracks) return;

        auto& track = loopTracks[(size_t) trackIndex];
        if (track) track->setPlaybackDirectionForward();
    }

    void setTrackPlaybackDirectionBackward (int trackIndex)
    {
        if (trackIndex < 0 || trackIndex >= numTracks) return;

        auto& track = loopTracks[(size_t) trackIndex];
        if (track) track->setPlaybackDirectionBackward();
    }

    float getTrackPlaybackSpeed (int trackIndex) const
    {
        if (trackIndex < 0 || trackIndex >= numTracks) return 1.0f;

        auto& track = loopTracks[(size_t) trackIndex];
        if (track) return track->getPlaybackSpeed();
        return 1.0f;
    }

    bool isTrackPlaybackForward (int trackIndex) const
    {
        if (trackIndex < 0 || trackIndex >= numTracks) return true;

        auto& track = loopTracks[(size_t) trackIndex];
        if (track) return track->isPlaybackDirectionForward();
        return true;
    }

    void setTrackVolume (int trackIndex, float volume);
    void setTrackMuted (int trackIndex, bool muted);
    void setTrackSoloed (int trackIndex, bool soloed);
    float getTrackVolume (int trackIndex) const;
    bool isTrackMuted (int trackIndex) const;
    void handleMidiCommand (const juce::MidiBuffer& midiMessages);

    void setKeepPitchWhenChangingSpeed (int trackIndex, bool shouldKeepPitch);
    bool getKeepPitchWhenChangingSpeed (int trackIndex) const;

    LooperState getState() const { return looperState; }
    bool hasPendingAction() const { return pendingAction.isActive(); }
    int getPendingTrackIndex() const;

    // NEW: State query methods (replace old isRecording/isPlaying/isStopped)
    bool isIdle() const { return looperState == LooperState::Idle; }
    bool isStopped() const { return looperState == LooperState::Stopped; }
    bool isPlaying() const
    {
        return looperState == LooperState::Playing || looperState == LooperState::PendingTrackChange
               || looperState == LooperState::Overdubbing;
    }
    bool isRecording() const { return looperState == LooperState::Recording || looperState == LooperState::Overdubbing; }

    void record();
    void play();
    void stop();
    void toggleRecord();
    void togglePlay();
    void undo (int trackIndex);
    void redo (int trackIndex);
    void clear (int trackIndex);

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

    LooperState looperState = LooperState::Idle;
    LooperState previousState = LooperState::Idle;
    PendingAction pendingAction;

    double sampleRate;
    int maxBlockSize;
    int numChannels;
    int numTracks;
    int activeTrackIndex;
    int nextTrackIndex;

    std::vector<std::unique_ptr<LoopTrack>> loopTracks;
    std::vector<std::unique_ptr<AudioToUIBridge>> uiBridges;
    std::vector<bool> bridgeInitialized;

    // State transition validation
    bool canTransitionTo (LooperState newState) const;
    void transitionTo (LooperState newState);

    // Pending action management
    void setPendingAction (PendingAction::Type type, int trackIndex, bool waitForWrap);
    void processPendingActions();

    std::unordered_map<MidiKey, std::function<void (LooperEngine&, int)>, MidiKeyHash> midiCommandMap;
    void setupMidiCommands();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEngine)
};
