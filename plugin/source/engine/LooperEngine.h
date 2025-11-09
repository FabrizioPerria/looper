#pragma once

#include "audio/EngineCommandBus.h"
#include "audio/EngineStateToUIBridge.h"
#include "engine/Constants.h"
#include "engine/GranularFreeze.h"
#include "engine/LevelMeter.h"
#include "engine/LoopTrack.h"
#include "engine/LooperStateConfig.h"
#include "engine/LooperStateMachine.h"
#include "engine/Metronome.h"
#include "engine/MidiCommandConfig.h"
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
    int targetTrackIndex = DEFAULT_ACTIVE_TRACK_INDEX;
    bool waitForWrapAround = false;
    LooperState previousState;

    void clear()
    {
        type = Type::None;
        targetTrackIndex = DEFAULT_ACTIVE_TRACK_INDEX;
        waitForWrapAround = false;
        previousState = LooperState::Idle;
    }

    bool isActive() const { return type != Type::None; }
};

class LooperEngine
{
public:
    LooperEngine();
    ~LooperEngine();

    void prepareToPlay (double sampleRate, int maxBlockSize, int numChannels);
    void releaseResources();

    void selectTrack (int trackIndex);

    void selectNextTrack() { selectTrack ((activeTrackIndex + 1) % numTracks); }
    void selectPreviousTrack() { selectTrack ((activeTrackIndex - 1 + numTracks) % numTracks); }

    int getNumTracks() const { return numTracks; }

    LoopTrack* getTrackByIndex (int trackIndex) const;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    // User actions
    void toggleRecord();
    void togglePlay();
    void toggleSync (int trackIndex);
    void toggleReverse (int trackIndex);
    void toggleSolo (int trackIndex);
    void toggleMute (int trackIndex);
    // void toggleVolumeNormalize (int trackIndex);
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

    Metronome* getMetronome() const { return metronome.get(); }
    bool shouldTrackPlay (int trackIndex) const;

    void toggleSinglePlayMode();
    bool isSinglePlayMode() const { return singlePlayMode.load(); }

    void toggleGranularFreeze();
    GranularFreeze* getGranularFreeze() const { return granularFreeze.get(); }

    MidiMappingManager* getMidiMappingManager() { return midiMappingManager.get(); }
    void saveMidiMappings() { midiMappingManager->saveToJson(); }

private:
    // State machine
    LooperStateMachine stateMachine;
    std::unique_ptr<MidiMappingManager> midiMappingManager = std::make_unique<MidiMappingManager>();
    LooperState currentState = LooperState::Idle;
    PendingAction pendingAction;
    std::unique_ptr<EngineStateToUIBridge> engineStateBridge = std::make_unique<EngineStateToUIBridge>();
    std::unique_ptr<EngineMessageBus> messageBus = std::make_unique<EngineMessageBus>();
    std::unique_ptr<Metronome> metronome = std::make_unique<Metronome>();
    std::unique_ptr<GranularFreeze> granularFreeze = std::make_unique<GranularFreeze>();

    std::unique_ptr<LevelMeter> inputMeter = std::make_unique<LevelMeter>();
    std::unique_ptr<LevelMeter> outputMeter = std::make_unique<LevelMeter>();

    std::atomic<float> inputGain { DEFAULT_INPUT_GAIN };
    std::atomic<float> outputGain { DEFAULT_OUTPUT_GAIN };

    std::atomic<bool> singlePlayMode { true };

    // Engine data
    double sampleRate = 0.0;
    int maxBlockSize = 0;
    int numChannels = 0;
    int numTracks = 0;
    int activeTrackIndex = 0;
    int nextTrackIndex = DEFAULT_ACTIVE_TRACK_INDEX;

    int syncMasterLength = 0;
    int syncMasterTrackIndex = DEFAULT_ACTIVE_TRACK_INDEX;

    std::array<std::unique_ptr<LoopTrack>, NUM_TRACKS> loopTracks;
    std::array<bool, NUM_TRACKS> tracksToPlay;

    // Helper methods
    bool trackHasContent (int index) const;
    LooperState determineStateAfterRecording() const;
    LooperState determineStateAfterStop() const;
    void switchToTrackImmediately (int trackIndex);
    void scheduleTrackSwitch (int trackIndex);

    bool transitionTo (LooperState newState);
    StateContext createStateContext (const juce::AudioBuffer<float>& buffer);

    void setPendingAction (PendingAction::Type type, int trackIndex, bool waitForWrap, LooperState currentState);
    void processPendingActions();
    void setupMidiCommands();
    void processCommandsFromMessageBus();

    void addTrack (int index);
    LoopTrack* getActiveTrack() const;
    void record();
    void play();
    void stop();
    void cancelRecording();
    void setKeepPitchWhenChangingSpeed (int trackIndex, bool shouldKeepPitch);
    bool getKeepPitchWhenChangingSpeed (int trackIndex) const;

    void setNewOverdubGainForTrack (int trackIndex, double newGain);
    void setExistingGainForTrack (int trackIndex, double oldGain);

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

    void toggleMetronomeEnabled();
    void setMetronomeBpm (int bpm);
    void setMetronomeTimeSignature (int numerator, int denominator);
    void setMetronomeStrongBeat (int beatIndex, bool isStrong);

    void setPlayheadPosition (int trackIndex, int positionSamples);
    void setLoopRegion (int trackIndex, int startSample, int endSample);
    void clearLoopRegion (int trackIndex);

    void saveTrackToFile (int trackIndex, const juce::File& audioFile);
    void saveAllTracksToFolder (const juce::File& folder);

    void handleMidiCommand (const juce::MidiBuffer& midiMessages, int trackIndex);
    EngineMessageBus::CommandPayload convertCCToCommand (const EngineMessageBus::CommandType ccId, const int value, int& trackIndex);

    const std::unordered_map<EngineMessageBus::CommandType, std::function<void (const EngineMessageBus::Command&)>> commandHandlers = {
        { EngineMessageBus::CommandType::TogglePlay,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) togglePlay();
          } },
        { EngineMessageBus::CommandType::ToggleRecord,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) toggleRecord();
          } },
        { EngineMessageBus::CommandType::Stop,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) stop();
          } },
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
        { EngineMessageBus::CommandType::NextTrack,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) selectNextTrack();
          } },
        { EngineMessageBus::CommandType::PreviousTrack,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) selectPreviousTrack();
          } },

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

        { EngineMessageBus::CommandType::ToggleMute,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) toggleMute (cmd.trackIndex);
          } },

        { EngineMessageBus::CommandType::ToggleSinglePlayMode,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) toggleSinglePlayMode();
          } },

        { EngineMessageBus::CommandType::ToggleFreeze,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) toggleGranularFreeze();
          } },

        { EngineMessageBus::CommandType::ToggleSolo,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) toggleSolo (cmd.trackIndex);
          } },

        { EngineMessageBus::CommandType::ToggleSyncTrack,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) toggleSync (cmd.trackIndex);
          } },

        // { EngineMessageBus::CommandType::ToggleVolumeNormalize,
        //   [this] (const auto& cmd)
        //   {
        //       if (std::holds_alternative<std::monostate> (cmd.payload)) toggleVolumeNormalize (cmd.trackIndex);
        //   } },

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

        { EngineMessageBus::CommandType::TogglePitchLock,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) toggleKeepPitchWhenChangingSpeed (cmd.trackIndex);
          } },

        { EngineMessageBus::CommandType::ToggleReverse,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) toggleReverse (cmd.trackIndex);
          } },

        { EngineMessageBus::CommandType::LoadAudioFile,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<juce::File> (cmd.payload))
              {
                  auto file = std::get<juce::File> (cmd.payload);
                  loadWaveFileToTrack (file, cmd.trackIndex);
              }
          } },
        { EngineMessageBus::CommandType::SetExistingAudioGain,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto gain = std::get<float> (cmd.payload);
                  setExistingGainForTrack (cmd.trackIndex, static_cast<double> (gain));
              }
          } },
        { EngineMessageBus::CommandType::SetNewOverdubGain,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto gain = std::get<float> (cmd.payload);
                  setNewOverdubGainForTrack (cmd.trackIndex, static_cast<double> (gain));
              }
          } },
        { EngineMessageBus::CommandType::ToggleMetronomeEnabled,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload))
              {
                  toggleMetronomeEnabled();
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
        { EngineMessageBus::CommandType::SetSubLoopRegion,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::pair<int, int>> (cmd.payload))
              {
                  auto region = std::get<std::pair<int, int>> (cmd.payload);
                  setLoopRegion (cmd.trackIndex, region.first, region.second);
              }
          } },
        { EngineMessageBus::CommandType::ClearSubLoopRegion,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload))
              {
                  clearLoopRegion (cmd.trackIndex);
              }
          } },

        { EngineMessageBus::CommandType::SetOutputGain,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto gain = std::get<float> (cmd.payload);
                  outputGain.store (gain);
              }
          } },
        { EngineMessageBus::CommandType::SetInputGain,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto gain = std::get<float> (cmd.payload);
                  inputGain.store (gain);
              }
          } },
        { EngineMessageBus::CommandType::SaveMidiMappings,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) midiMappingManager->saveToJson();
          } },
        { EngineMessageBus::CommandType::LoadMidiMappings,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) midiMappingManager->loadFromJson();
          } },
        { EngineMessageBus::CommandType::ResetMidiMappings,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) midiMappingManager->resetToDefaults();
          } },
        { EngineMessageBus::CommandType::StartMidiLearn,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<int> (cmd.payload))
              {
                  auto commandId = static_cast<EngineMessageBus::CommandType> (std::get<int> (cmd.payload));
                  midiMappingManager->startMidiLearn (commandId);
              }
          } },
        { EngineMessageBus::CommandType::StopMidiLearn,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) midiMappingManager->stopMidiLearn();
          } },
        { EngineMessageBus::CommandType::CancelMidiLearn,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) midiMappingManager->stopMidiLearn();
          } },
        { EngineMessageBus::CommandType::ClearMidiMappings,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<std::monostate> (cmd.payload)) midiMappingManager->clearAllMappings();
          } },
        { EngineMessageBus::CommandType::SetFreezeLevel,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<float> (cmd.payload))
              {
                  auto level = std::get<float> (cmd.payload);
                  granularFreeze->setLevel (level);
              }
          } },
        { EngineMessageBus::CommandType::SaveTrackToFile,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<juce::File> (cmd.payload))
              {
                  auto file = std::get<juce::File> (cmd.payload);
                  saveTrackToFile (cmd.trackIndex, file);
              }
          } },
        { EngineMessageBus::CommandType::SaveAllTracksToFolder,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<juce::File> (cmd.payload))
              {
                  auto folder = std::get<juce::File> (cmd.payload);
                  saveAllTracksToFolder (folder);
              }
          } },
        { EngineMessageBus::CommandType::SetPlayheadPosition,
          [this] (const auto& cmd)
          {
              if (std::holds_alternative<int> (cmd.payload))
              {
                  auto position = std::get<int> (cmd.payload);
                  setPlayheadPosition (cmd.trackIndex, position);
              }
          } },

    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEngine)
};
