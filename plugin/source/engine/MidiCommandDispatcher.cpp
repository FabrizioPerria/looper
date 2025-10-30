#include "MidiCommandDispatcher.h"
#include "LooperEngine.h"

#include "MidiCommandDispatcher.h"

namespace CommandExecutors
{
static void executeToggleRecord (LooperEngine& engine, int) { engine.toggleRecord(); }
static void executeTogglePlay (LooperEngine& engine, int) { engine.togglePlay(); }
static void executeUndo (LooperEngine& engine, int trackIndex) { engine.undo (trackIndex); }
static void executeRedo (LooperEngine& engine, int trackIndex) { engine.redo (trackIndex); }
static void executeClear (LooperEngine& engine, int trackIndex) { engine.clear (trackIndex); }
static void executeNextTrack (LooperEngine& engine, int) { engine.selectNextTrack(); }
static void executePrevTrack (LooperEngine& engine, int) { engine.selectPreviousTrack(); }
static void executeToggleSolo (LooperEngine& engine, int trackIndex) { engine.toggleSolo (trackIndex); }
static void executeToggleMute (LooperEngine& engine, int trackIndex) { engine.toggleMute (trackIndex); }
static void executeToggleReverse (LooperEngine& engine, int trackIndex) { engine.toggleReverse (trackIndex); }

static void executeToggleKeepPitch (LooperEngine& engine, int trackIndex) { engine.toggleKeepPitchWhenChangingSpeed (trackIndex); }

static void executeVolumeNormalize (LooperEngine& engine, int trackIndex) { engine.toggleVolumeNormalize (trackIndex); }

static void executeSelectTrack (LooperEngine& engine, int, int value)
{
    int numTracks = engine.getNumTracks();
    int targetTrack = juce::jlimit (0, numTracks - 1, value % numTracks);
    engine.selectTrack (targetTrack);
}
static void executeSetTrackVolume (LooperEngine& engine, int trackIndex, int value)
{
    float volume = (float) value / 127.0f;
    engine.setTrackVolume (trackIndex, volume);
}
static void executeSetPlaybackSpeed (LooperEngine& engine, int trackIndex, int value)
{
    float normalizedValue = (float) value / 127.0f;
    float speed = 0.5f + (normalizedValue * 1.5f);
    engine.setTrackPlaybackSpeed (trackIndex, speed);
}
static void executeSetOverdubGain (LooperEngine& engine, int trackIndex, int value)
{
    float normalizedValue = (float) value / 127.0f;
    double gain = juce::jmap (normalizedValue, 0.0f, 2.0f);
    auto* track = engine.getTrackByIndex (trackIndex);
    if (track) track->setOverdubGainNew (gain);
}
static void executeSetOldOverdubGain (LooperEngine& engine, int trackIndex, int value)
{
    float normalizedValue = (float) value / 127.0f;
    double gain = juce::jmap (normalizedValue, 0.0f, 2.0f);
    auto* track = engine.getTrackByIndex (trackIndex);
    if (track) track->setOverdubGainOld (gain);
}

static void executePitchShift (LooperEngine& engine, int trackIndex, int value)
{
    float semitones = juce::jmap ((float) value, 0.0f, 127.0f, -2.0f, 2.0f);
    engine.setTrackPitch (trackIndex, semitones);
}

static void executeNone (LooperEngine&, int) {}
static void executeNone (LooperEngine&, int, int) {}
} // namespace CommandExecutors

const CommandFunc COMMAND_DISPATCH_TABLE[(size_t) MidiCommandId::COUNT] = {
    [(size_t) MidiCommandId::None] = CommandExecutors::executeNone,
    [(size_t) MidiCommandId::ToggleRecord] = CommandExecutors::executeToggleRecord,
    [(size_t) MidiCommandId::TogglePlay] = CommandExecutors::executeTogglePlay,
    [(size_t) MidiCommandId::Undo] = CommandExecutors::executeUndo,
    [(size_t) MidiCommandId::Redo] = CommandExecutors::executeRedo,
    [(size_t) MidiCommandId::Clear] = CommandExecutors::executeClear,
    [(size_t) MidiCommandId::NextTrack] = CommandExecutors::executeNextTrack,
    [(size_t) MidiCommandId::PrevTrack] = CommandExecutors::executePrevTrack,
    [(size_t) MidiCommandId::ToggleSolo] = CommandExecutors::executeToggleSolo,
    [(size_t) MidiCommandId::ToggleMute] = CommandExecutors::executeToggleMute,
    [(size_t) MidiCommandId::ToggleReverse] = CommandExecutors::executeToggleReverse,
    [(size_t) MidiCommandId::ToggleKeepPitch] = CommandExecutors::executeToggleKeepPitch,
    [(size_t) MidiCommandId::VolumeNormalize] = CommandExecutors::executeVolumeNormalize,
};

void MidiCommandDispatcher::dispatch (MidiCommandId commandId, LooperEngine& engine, int trackIndex)
{
    if (commandId == MidiCommandId::None || commandId >= MidiCommandId::COUNT) return;

    COMMAND_DISPATCH_TABLE[static_cast<size_t> (commandId)](engine, trackIndex);
}

const ControlChangeFunc COMMAND_CONTROL_CHANGE_DISPATCH_TABLE[static_cast<size_t> (MidiControlChangeId::COUNT)] = {
    [(size_t) MidiControlChangeId::None] = CommandExecutors::executeNone,
    [(size_t) MidiControlChangeId::TrackSelect] = CommandExecutors::executeSelectTrack,
    [(size_t) MidiControlChangeId::TrackVolume] = CommandExecutors::executeSetTrackVolume,
    [(size_t) MidiControlChangeId::PlaybackSpeed] = CommandExecutors::executeSetPlaybackSpeed,
    [(size_t) MidiControlChangeId::OverdubLevel] = CommandExecutors::executeSetOverdubGain,
    [(size_t) MidiControlChangeId::ExistingAudioLevel] = CommandExecutors::executeSetOldOverdubGain,
    [(size_t) MidiControlChangeId::PitchShift] = CommandExecutors::executePitchShift,
};

void MidiCommandDispatcher::dispatch (MidiControlChangeId commandId, LooperEngine& engine, int trackIndex, int param)
{
    if (commandId == MidiControlChangeId::None || commandId >= MidiControlChangeId::COUNT) return;

    COMMAND_CONTROL_CHANGE_DISPATCH_TABLE[static_cast<size_t> (commandId)](engine, trackIndex, param);
}
