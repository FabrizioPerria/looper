#pragma once

#include "audio/AudioToUIBridge.h"
#include "engine/LoopTrack.h"
#include "engine/LooperStateMachine.h"
#include "engine/MidiCommandConfig.h"
#include <JuceHeader.h>

struct PendingAction
{
    enum class Type : uint8_t
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
    void selectTrack (int trackIndex);
    void removeTrack (int trackIndex);

    void selectNextTrack() { selectTrack ((activeTrackIndex + 1) % numTracks); }
    void selectPreviousTrack() { selectTrack ((activeTrackIndex - 1 + numTracks) % numTracks); }

    int getActiveTrackIndex() const { return activeTrackIndex; }
    int getNumTracks() const { return numTracks; }
    int trackBeingChanged() const { return nextTrackIndex >= 0 ? nextTrackIndex : activeTrackIndex; }

    LoopTrack* getActiveTrack() const;
    LoopTrack* getTrackByIndex (int trackIndex);
    AudioToUIBridge* getUIBridgeByIndex (int trackIndex);

    void processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    // State queries
    LooperState getState() const { return currentState; }
    bool hasPendingAction() const { return pendingAction.isActive(); }
    int getPendingTrackIndex() const;

    bool isIdle() const { return currentState == LooperState::Idle; }
    bool isStopped() const { return currentState == LooperState::Stopped; }
    bool isPlaying() const { return StateConfig::isPlaying (currentState); }
    bool isRecording() const { return StateConfig::isRecording (currentState); }

    // User actions
    void record();
    void play();
    void stop();
    void toggleRecord();
    void togglePlay();
    void undo (int trackIndex);
    void redo (int trackIndex);
    void clear (int trackIndex);

    // Track controls
    void setOverdubGainsForTrack (int trackIndex, double oldGain, double newGain);
    void loadBackingTrackToTrack (const juce::AudioBuffer<float>& backingTrack, int trackIndex);
    void loadWaveFileToTrack (const juce::File& audioFile, int trackIndex);

    void setTrackPlaybackSpeed (int trackIndex, float speed);
    void setTrackPlaybackDirectionForward (int trackIndex);
    void setTrackPlaybackDirectionBackward (int trackIndex);
    float getTrackPlaybackSpeed (int trackIndex) const;
    bool isTrackPlaybackForward (int trackIndex) const;

    void setTrackVolume (int trackIndex, float volume);
    void setTrackMuted (int trackIndex, bool muted);
    void setTrackSoloed (int trackIndex, bool soloed);
    float getTrackVolume (int trackIndex) const;
    bool isTrackMuted (int trackIndex) const;

    void setKeepPitchWhenChangingSpeed (int trackIndex, bool shouldKeepPitch);
    bool getKeepPitchWhenChangingSpeed (int trackIndex) const;

    void handleMidiCommand (const juce::MidiBuffer& midiMessages);

private:
    // State machine
    LooperStateMachine stateMachine;
    LooperState currentState = LooperState::Idle;
    PendingAction pendingAction;

    // Engine data
    double sampleRate = 0.0;
    int maxBlockSize = 0;
    int numChannels = 0;
    int numTracks = 0;
    int activeTrackIndex = 0;
    int nextTrackIndex = -1;

    std::vector<std::unique_ptr<LoopTrack>> loopTracks;
    std::vector<std::unique_ptr<AudioToUIBridge>> uiBridges;
    std::vector<bool> bridgeInitialized;

    // Helper methods
    bool trackHasContent() const;
    LooperState determineStateAfterRecording() const;
    LooperState determineStateAfterStop() const;
    void switchToTrackImmediately (int trackIndex);
    void scheduleTrackSwitch (int trackIndex);

    bool transitionTo (LooperState newState);
    StateContext createStateContext (const juce::AudioBuffer<float>& buffer);
    void updateUIBridge (StateContext& ctx, bool wasRecording);
    int calculateLengthToShow (const LoopTrack* track, bool isRecording) const;

    void setPendingAction (PendingAction::Type type, int trackIndex, bool waitForWrap);
    void processPendingActions();
    void setupMidiCommands();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEngine)
};
