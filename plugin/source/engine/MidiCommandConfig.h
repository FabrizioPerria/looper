#pragma once
#include "audio/EngineCommandBus.h"
#include <array>
#include <cstdint>

namespace MidiNotes
{
constexpr uint8_t TOGGLE_RECORD_BUTTON = 60;
constexpr uint8_t TOGGLE_PLAY_BUTTON = 61;
constexpr uint8_t STOP_BUTTON = 62;
constexpr uint8_t SYNC_BUTTON = 63;
constexpr uint8_t SINGLE_PLAY_MODE_BUTTON = 64;
constexpr uint8_t FREEZE_BUTTON = 65;
constexpr uint8_t UNDO_BUTTON = 66;
constexpr uint8_t REDO_BUTTON = 67;
constexpr uint8_t CLEAR_BUTTON = 68;
constexpr uint8_t NEXT_TRACK = 69;
constexpr uint8_t PREV_TRACK = 70;
constexpr uint8_t MUTE_BUTTON = 71;
constexpr uint8_t SOLO_BUTTON = 72;
constexpr uint8_t NORMALIZE_BUTTON = 73;
constexpr uint8_t PITCH_LOCK_BUTTON = 74;
constexpr uint8_t REVERSE_BUTTON = 75;
constexpr uint8_t METRO_TOGGLE_BUTTON = 76;
constexpr uint8_t METRO_STRONG_BEAT_BUTTON = 77;

constexpr uint8_t TRACK_SELECT_CC = 102;
constexpr uint8_t TRACK_VOLUME_CC = 7;
constexpr uint8_t PLAYBACK_SPEED_CC = 1;
constexpr uint8_t PITCH_SHIFT_CC = 14;
constexpr uint8_t EXISTING_AUDIO_LEVEL_CC = 104;
constexpr uint8_t OVERDUB_LEVEL_CC = 103;
constexpr uint8_t METRONOME_BPM_CC = 100;
constexpr uint8_t METRONOME_VOLUME_CC = 105;
constexpr uint8_t INPUT_GAIN_CC = 108;
constexpr uint8_t OUTPUT_GAIN_CC = 109;

} // namespace MidiNotes

namespace MidiCommandMapping
{
constexpr size_t MAX_MIDI_NOTES = 128;

// Build lookup table at compile time
constexpr std::array<EngineMessageBus::CommandType, MAX_MIDI_NOTES> buildNoteOnCommands()
{
    std::array<EngineMessageBus::CommandType, MAX_MIDI_NOTES> table {};

    // Initialize all to None
    for (size_t i = 0; i < MAX_MIDI_NOTES; ++i)
        table[i] = EngineMessageBus::CommandType::Stop;

    // Map specific notes to commands
    table[MidiNotes::TOGGLE_RECORD_BUTTON] = EngineMessageBus::CommandType::ToggleRecord;
    table[MidiNotes::TOGGLE_PLAY_BUTTON] = EngineMessageBus::CommandType::TogglePlay;
    table[MidiNotes::STOP_BUTTON] = EngineMessageBus::CommandType::Stop;
    table[MidiNotes::SYNC_BUTTON] = EngineMessageBus::CommandType::ToggleSyncTrack;
    table[MidiNotes::SINGLE_PLAY_MODE_BUTTON] = EngineMessageBus::CommandType::ToggleSinglePlayMode;
    table[MidiNotes::FREEZE_BUTTON] = EngineMessageBus::CommandType::ToggleFreeze;
    table[MidiNotes::UNDO_BUTTON] = EngineMessageBus::CommandType::Undo;
    table[MidiNotes::REDO_BUTTON] = EngineMessageBus::CommandType::Redo;
    table[MidiNotes::CLEAR_BUTTON] = EngineMessageBus::CommandType::Clear;
    table[MidiNotes::NEXT_TRACK] = EngineMessageBus::CommandType::NextTrack;
    table[MidiNotes::PREV_TRACK] = EngineMessageBus::CommandType::PreviousTrack;
    table[MidiNotes::MUTE_BUTTON] = EngineMessageBus::CommandType::ToggleMute;
    table[MidiNotes::SOLO_BUTTON] = EngineMessageBus::CommandType::ToggleSolo;
    table[MidiNotes::NORMALIZE_BUTTON] = EngineMessageBus::CommandType::ToggleVolumeNormalize;
    table[MidiNotes::PITCH_LOCK_BUTTON] = EngineMessageBus::CommandType::TogglePitchLock;
    table[MidiNotes::REVERSE_BUTTON] = EngineMessageBus::CommandType::ToggleReverse;
    table[MidiNotes::METRO_TOGGLE_BUTTON] = EngineMessageBus::CommandType::SetMetronomeEnabled;
    table[MidiNotes::METRO_STRONG_BEAT_BUTTON] = EngineMessageBus::CommandType::SetMetronomeStrongBeat;

    return table;
}

constexpr std::array<EngineMessageBus::CommandType, MAX_MIDI_NOTES> buildNoteOffCommands()
{
    std::array<EngineMessageBus::CommandType, MAX_MIDI_NOTES> table {};

    // Initialize all to None
    for (size_t i = 0; i < MAX_MIDI_NOTES; ++i)
        table[i] = EngineMessageBus::CommandType::Stop;

    // Currently no note-off commands, but structure is here if needed

    return table;
}

constexpr auto NOTE_ON_COMMANDS = buildNoteOnCommands();
constexpr auto NOTE_OFF_COMMANDS = buildNoteOffCommands();

constexpr EngineMessageBus::CommandType getCommandForNoteOn (uint8_t note)
{
    return note < MAX_MIDI_NOTES ? NOTE_ON_COMMANDS[note] : EngineMessageBus::CommandType::Stop;
}
constexpr EngineMessageBus::CommandType getCommandForNoteOff (uint8_t note)
{
    return note < MAX_MIDI_NOTES ? NOTE_OFF_COMMANDS[note] : EngineMessageBus::CommandType::Stop;
}

} // namespace MidiCommandMapping

namespace MidiControlChangeMapping
{
constexpr size_t MAX_CC_NUMBERS = 128;
// Build lookup table at compile time
constexpr std::array<EngineMessageBus::CommandType, MAX_CC_NUMBERS> buildCCMapping()
{
    std::array<EngineMessageBus::CommandType, MAX_CC_NUMBERS> table {};
    // Initialize all to COUNT (invalid)
    for (size_t i = 0; i < MAX_CC_NUMBERS; ++i)
        table[i] = EngineMessageBus::CommandType::Stop;
    // Map specific CC numbers to control change IDs
    table[MidiNotes::TRACK_SELECT_CC] = EngineMessageBus::CommandType::SelectTrack;
    table[MidiNotes::TRACK_VOLUME_CC] = EngineMessageBus::CommandType::SetVolume;
    table[MidiNotes::PLAYBACK_SPEED_CC] = EngineMessageBus::CommandType::SetPlaybackSpeed;
    table[MidiNotes::PITCH_SHIFT_CC] = EngineMessageBus::CommandType::SetPlaybackPitch;
    table[MidiNotes::EXISTING_AUDIO_LEVEL_CC] = EngineMessageBus::CommandType::SetExistingAudioGain;
    table[MidiNotes::OVERDUB_LEVEL_CC] = EngineMessageBus::CommandType::SetNewOverdubGain;
    table[MidiNotes::METRONOME_BPM_CC] = EngineMessageBus::CommandType::SetMetronomeBPM;
    table[MidiNotes::METRONOME_VOLUME_CC] = EngineMessageBus::CommandType::SetMetronomeVolume;
    table[MidiNotes::INPUT_GAIN_CC] = EngineMessageBus::CommandType::SetInputGain;
    table[MidiNotes::OUTPUT_GAIN_CC] = EngineMessageBus::CommandType::SetOutputGain;
    return table;
}

constexpr auto CC_MAPPING = buildCCMapping();
constexpr EngineMessageBus::CommandType getControlChangeId (uint8_t ccNumber)
{
    return ccNumber < MAX_CC_NUMBERS ? CC_MAPPING[ccNumber] : EngineMessageBus::CommandType::Stop;
}

} // namespace MidiControlChangeMapping
