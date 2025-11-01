#pragma once

#include "audio/AudioToUIBridge.h"
#include "audio/EngineCommandBus.h"
#include "audio/EngineStateToUIBridge.h"
#include "engine/LoopTrack.h"
#include "engine/LooperStateMachine.h"
#include "engine/Metronome.h"
#include <JuceHeader.h>

struct PendingAction
{
    enum class Type : uint8_t
    {
        None,
        SwitchTrack,
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

    void selectTrack (int trackIndex);

    void selectNextTrack() { selectTrack ((activeTrackIndex + 1) % numTracks); }
    void selectPreviousTrack() { selectTrack ((activeTrackIndex - 1 + numTracks) % numTracks); }

    int getNumTracks() const { return numTracks; }

    LoopTrack* getTrackByIndex (int trackIndex) const;
    AudioToUIBridge* getUIBridgeByIndex (int trackIndex);

    void processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    // User actions
    void toggleRecord();
    void togglePlay();
    void toggleReverse (int trackIndex);
    void toggleSolo (int trackIndex);
    void toggleMute (int trackIndex);
    void toggleVolumeNormalize (int trackIndex);
    void toggleKeepPitchWhenChangingSpeed (int trackIndex);
    void undo (int trackIndex);
    void redo (int trackIndex);
    void clear (int trackIndex);

    void setTrackPlaybackSpeed (int trackIndex, float speed);

    void setTrackVolume (int trackIndex, float volume);
    void setTrackPitch (int trackIndex, float pitch);
    void setMetronomeVolume (float volume);

    EngineStateToUIBridge* getEngineStateBridge() const { return engineStateBridge.get(); }
    EngineMessageBus* getMessageBus() const { return messageBus.get(); }

private:
    // State machine
    LooperStateMachine stateMachine;
    LooperState currentState = LooperState::Idle;
    PendingAction pendingAction;
    std::unique_ptr<EngineStateToUIBridge> engineStateBridge = std::make_unique<EngineStateToUIBridge>();
    std::unique_ptr<EngineMessageBus> messageBus = std::make_unique<EngineMessageBus>();

    std::unique_ptr<Metronome> metronome = std::make_unique<Metronome>();

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
    void processCommandsFromMessageBus();

    bool shouldTrackPlay (int trackIndex) const;

    void addTrack();
    void removeTrack (int trackIndex);
    LoopTrack* getActiveTrack() const;
    void record();
    void play();
    void stop();
    void cancelRecording();
    void setKeepPitchWhenChangingSpeed (int trackIndex, bool shouldKeepPitch);
    bool getKeepPitchWhenChangingSpeed (int trackIndex) const;
    void setOverdubGainsForTrack (int trackIndex, double oldGain, double newGain);
    void loadBackingTrackToTrack (const juce::AudioBuffer<float>& backingTrack, int trackIndex);
    void loadWaveFileToTrack (const juce::File& audioFile, int trackIndex);
    void setTrackPlaybackDirectionForward (int trackIndex);
    void setTrackPlaybackDirectionBackward (int trackIndex);
    float getTrackPlaybackSpeed (int trackIndex) const;
    bool isTrackPlaybackForward (int trackIndex) const;

    void setTrackMuted (int trackIndex, bool muted);
    void setTrackSoloed (int trackIndex, bool soloed);
    float getTrackVolume (int trackIndex) const;
    bool isTrackMuted (int trackIndex) const;

    void enableMetronome (bool enable);
    void setMetronomeBpm (int bpm);
    void setMetronomeTimeSignature (int numerator, int denominator);
    void setMetronomeStrongBeat (int beatIndex, bool isStrong);

    void handleMidiCommand (const juce::MidiBuffer& midiMessages);

    const std::unordered_map<EngineMessageBus::CommandType, std::function<void (const EngineMessageBus::Command&)>> commandHandlers = {
        { EngineMessageBus::CommandType::TogglePlay, [this] (const auto& /*cmd*/) { togglePlay(); } },
        { EngineMessageBus::CommandType::ToggleRecord, [this] (const auto& /*cmd*/) { toggleRecord(); } },
        { EngineMessageBus::CommandType::Undo,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) undo (cmd.trackIndex);
          } },
        { EngineMessageBus::CommandType::Redo,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) redo (cmd.trackIndex);
          } },
        { EngineMessageBus::CommandType::Clear,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) clear (cmd.trackIndex);
          } },
        { EngineMessageBus::CommandType::NextTrack, [this] (const auto& /*cmd*/) { selectNextTrack(); } },
        { EngineMessageBus::CommandType::PreviousTrack, [this] (const auto& /*cmd*/) { selectPreviousTrack(); } },

        { EngineMessageBus::CommandType::SelectTrack,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) selectTrack (cmd.trackIndex);
          } },

        { EngineMessageBus::CommandType::SetVolume,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto volume = std::get<float> (cmd.payload);
                  setTrackVolume (cmd.trackIndex, volume);
              }
          } },

        { EngineMessageBus::CommandType::ToggleMute, [this] (const auto& cmd) { toggleMute (cmd.trackIndex); } },

        { EngineMessageBus::CommandType::ToggleSolo, [this] (const auto& cmd) { toggleSolo (cmd.trackIndex); } },

        { EngineMessageBus::CommandType::ToggleVolumeNormalize, [this] (const auto& cmd) { toggleVolumeNormalize (cmd.trackIndex); } },

        { EngineMessageBus::CommandType::SetPlaybackSpeed,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto speed = std::get<float> (cmd.payload);
                  setTrackPlaybackSpeed (cmd.trackIndex, speed);
              }
          } },

        { EngineMessageBus::CommandType::SetPlaybackPitch,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto pitch = std::get<float> (cmd.payload);
                  setTrackPitch (cmd.trackIndex, pitch);
              }
          } },

        { EngineMessageBus::CommandType::TogglePitchLock, [this] (const auto& cmd) { toggleKeepPitchWhenChangingSpeed (cmd.trackIndex); } },

        { EngineMessageBus::CommandType::ToggleReverse, [this] (const auto& cmd) { toggleReverse (cmd.trackIndex); } },

        { EngineMessageBus::CommandType::LoadAudioFile,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<juce::File> (cmd.payload))
              {
                  auto file = std::get<juce::File> (cmd.payload);
                  loadWaveFileToTrack (file, cmd.trackIndex);
              }
          } },
        { EngineMessageBus::CommandType::SetOverdubGains,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::pair<float, float>> (cmd.payload))
              {
                  auto gains = std::get<std::pair<float, float>> (cmd.payload);
                  setOverdubGainsForTrack (cmd.trackIndex, gains.first, gains.second);
              }
          } },
        { EngineMessageBus::CommandType::MidiMessage,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<juce::MidiBuffer> (cmd.payload))
              {
                  auto midiBuffer = std::get<juce::MidiBuffer> (cmd.payload);
                  handleMidiCommand (midiBuffer);
              }
          } },
        { EngineMessageBus::CommandType::SetMetronomeEnabled,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<bool> (cmd.payload))
              {
                  auto enabled = std::get<bool> (cmd.payload);
                  enableMetronome (enabled);
              }
          } },
        { EngineMessageBus::CommandType::SetMetronomeBPM,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<int> (cmd.payload))
              {
                  auto bpm = static_cast<int> (std::get<int> (cmd.payload));
                  setMetronomeBpm (bpm);
              }
          } },
        { EngineMessageBus::CommandType::SetMetronomeTimeSignature,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::pair<int, int>> (cmd.payload))
              {
                  auto ts = std::get<std::pair<int, int>> (cmd.payload);
                  setMetronomeTimeSignature (static_cast<int> (ts.first), static_cast<int> (ts.second));
              }
          } },
        { EngineMessageBus::CommandType::SetMetronomeStrongBeat,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<int> (cmd.payload))
              {
                  auto beatInfo = static_cast<int> (std::get<int> (cmd.payload));
                  if (beatInfo >= 0) setMetronomeStrongBeat (static_cast<int> (beatInfo), beatInfo > 0);
              }
          } },
        { EngineMessageBus::CommandType::SetMetronomeVolume,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto volume = std::get<float> (cmd.payload);
                  setMetronomeVolume (volume);
              }
          } },

    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEngine)
};
