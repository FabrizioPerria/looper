#pragma once
#include <array>
#include <cstdint>

namespace MidiNotes
{
constexpr uint8_t TOGGLE_RECORD_BUTTON = 60;
constexpr uint8_t TOGGLE_PLAY_BUTTON = 61;
constexpr uint8_t UNDO_BUTTON = 62;
constexpr uint8_t REDO_BUTTON = 63;
constexpr uint8_t CLEAR_BUTTON = 64;
constexpr uint8_t NEXT_TRACK = 65;
constexpr uint8_t PREV_TRACK = 66;
constexpr uint8_t SOLO_BUTTON = 67;
constexpr uint8_t MUTE_BUTTON = 68;
constexpr uint8_t LOAD_BUTTON = 69;
constexpr uint8_t REVERSE_BUTTON = 70;
constexpr uint8_t KEEP_PITCH_BUTTON = 71;

constexpr uint8_t TRACK_SELECT_CC = 102;
constexpr uint8_t TRACK_VOLUME_CC = 7;
constexpr uint8_t PLAYBACK_SPEED_CC = 1;
} // namespace MidiNotes

// MIDI command IDs - enum for type safety
enum class MidiCommandId : uint8_t
{
    None = 0,
    ToggleRecord,
    TogglePlay,
    Undo,
    Redo,
    Clear,
    NextTrack,
    PrevTrack,
    ToggleSolo,
    ToggleMute,
    LoadFile,
    ToggleReverse,
    ToggleKeepPitch,

    COUNT
};

namespace MidiCommandMapping
{
constexpr size_t MAX_MIDI_NOTES = 128;

// Build lookup table at compile time
constexpr std::array<MidiCommandId, MAX_MIDI_NOTES> buildNoteOnCommands()
{
    std::array<MidiCommandId, MAX_MIDI_NOTES> table{};

    // Initialize all to None
    for (size_t i = 0; i < MAX_MIDI_NOTES; ++i)
        table[i] = MidiCommandId::None;

    // Map specific notes to commands
    table[MidiNotes::TOGGLE_RECORD_BUTTON] = MidiCommandId::ToggleRecord;
    table[MidiNotes::TOGGLE_PLAY_BUTTON] = MidiCommandId::TogglePlay;
    table[MidiNotes::UNDO_BUTTON] = MidiCommandId::Undo;
    table[MidiNotes::REDO_BUTTON] = MidiCommandId::Redo;
    table[MidiNotes::CLEAR_BUTTON] = MidiCommandId::Clear;
    table[MidiNotes::NEXT_TRACK] = MidiCommandId::NextTrack;
    table[MidiNotes::PREV_TRACK] = MidiCommandId::PrevTrack;
    table[MidiNotes::SOLO_BUTTON] = MidiCommandId::ToggleSolo;
    table[MidiNotes::MUTE_BUTTON] = MidiCommandId::ToggleMute;
    table[MidiNotes::LOAD_BUTTON] = MidiCommandId::LoadFile;
    table[MidiNotes::REVERSE_BUTTON] = MidiCommandId::ToggleReverse;
    table[MidiNotes::KEEP_PITCH_BUTTON] = MidiCommandId::ToggleKeepPitch;

    return table;
}

constexpr std::array<MidiCommandId, MAX_MIDI_NOTES> buildNoteOffCommands()
{
    std::array<MidiCommandId, MAX_MIDI_NOTES> table{};

    // Initialize all to None
    for (size_t i = 0; i < MAX_MIDI_NOTES; ++i)
        table[i] = MidiCommandId::None;

    // Currently no note-off commands, but structure is here if needed

    return table;
}

constexpr auto NOTE_ON_COMMANDS = buildNoteOnCommands();
constexpr auto NOTE_OFF_COMMANDS = buildNoteOffCommands();

constexpr MidiCommandId getCommandForNoteOn(uint8_t note)
{
    return note < MAX_MIDI_NOTES ? NOTE_ON_COMMANDS[note] : MidiCommandId::None;
}
constexpr MidiCommandId getCommandForNoteOff(uint8_t note)
{
    return note < MAX_MIDI_NOTES ? NOTE_OFF_COMMANDS[note] : MidiCommandId::None;
}

struct CommandFlags
{
    bool needsTrackIndex : 1;
    bool canRunDuringRecording : 1;
};

constexpr CommandFlags COMMAND_FLAGS[static_cast<size_t>(MidiCommandId::COUNT)] = {
    [(size_t)MidiCommandId::None] = CommandFlags{false, true},
    [(size_t)MidiCommandId::ToggleRecord] = CommandFlags{false, true},
    [(size_t)MidiCommandId::TogglePlay] = CommandFlags{false, true},
    [(size_t)MidiCommandId::Undo] = CommandFlags{true, false},
    [(size_t)MidiCommandId::Redo] = CommandFlags{true, false},
    [(size_t)MidiCommandId::Clear] = CommandFlags{true, false},
    [(size_t)MidiCommandId::NextTrack] = CommandFlags{false, true},
    [(size_t)MidiCommandId::PrevTrack] = CommandFlags{false, true},
    [(size_t)MidiCommandId::ToggleSolo] = CommandFlags{true, true},
    [(size_t)MidiCommandId::ToggleMute] = CommandFlags{true, true},
    [(size_t)MidiCommandId::LoadFile] = CommandFlags{true, false},
    [(size_t)MidiCommandId::ToggleReverse] = CommandFlags{true, true},
    [(size_t)MidiCommandId::ToggleKeepPitch] = CommandFlags{true, true}};

constexpr bool needsTrackIndex(MidiCommandId cmd)
{
    return COMMAND_FLAGS[static_cast<size_t>(cmd)].needsTrackIndex;
}

constexpr bool canRunDuringRecording(MidiCommandId cmd)
{
    return COMMAND_FLAGS[static_cast<size_t>(cmd)].canRunDuringRecording;
}
} // namespace MidiCommandMapping
