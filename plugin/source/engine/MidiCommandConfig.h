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
constexpr uint8_t VOLUME_NORMALIZE_BUTTON = 72;

constexpr uint8_t TRACK_SELECT_CC = 102;
constexpr uint8_t TRACK_VOLUME_CC = 7;
constexpr uint8_t PLAYBACK_SPEED_CC = 1;
constexpr uint8_t OVERDUB_LEVEL_CC = 103;
constexpr uint8_t EXISTING_AUDIO_LEVEL_CC = 104;
constexpr uint8_t PITCH_SHIFT_CC = 14;
constexpr uint8_t METRONOME_VOLUME_CC = 105;
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
    VolumeNormalize,

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
    table[MidiNotes::VOLUME_NORMALIZE_BUTTON] = MidiCommandId::VolumeNormalize;

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
    [(size_t)MidiCommandId::ToggleKeepPitch] = CommandFlags{true, true},
    [(size_t)MidiCommandId::VolumeNormalize] = CommandFlags{true, true},
};

constexpr bool needsTrackIndex(MidiCommandId cmd)
{
    return COMMAND_FLAGS[static_cast<size_t>(cmd)].needsTrackIndex;
}

constexpr bool canRunDuringRecording(MidiCommandId cmd)
{
    return COMMAND_FLAGS[static_cast<size_t>(cmd)].canRunDuringRecording;
}

} // namespace MidiCommandMapping

enum class MidiControlChangeId : uint8_t
{
    None = 0,
    TrackSelect = MidiNotes::TRACK_SELECT_CC,
    TrackVolume = MidiNotes::TRACK_VOLUME_CC,
    PlaybackSpeed = MidiNotes::PLAYBACK_SPEED_CC,
    OverdubLevel = MidiNotes::OVERDUB_LEVEL_CC,
    PitchShift = MidiNotes::PITCH_SHIFT_CC,
    ExistingAudioLevel = MidiNotes::EXISTING_AUDIO_LEVEL_CC,
    MetronomeVolume = MidiNotes::METRONOME_VOLUME_CC,

    COUNT
};

namespace MidiControlChangeMapping
{
constexpr size_t MAX_CC_NUMBERS = 128;
// Build lookup table at compile time
constexpr std::array<MidiControlChangeId, MAX_CC_NUMBERS> buildCCMapping()
{
    std::array<MidiControlChangeId, MAX_CC_NUMBERS> table{};
    // Initialize all to COUNT (invalid)
    for (size_t i = 0; i < MAX_CC_NUMBERS; ++i)
        table[i] = MidiControlChangeId::COUNT;
    // Map specific CC numbers to control change IDs
    table[MidiNotes::TRACK_SELECT_CC] = MidiControlChangeId::TrackSelect;
    table[MidiNotes::TRACK_VOLUME_CC] = MidiControlChangeId::TrackVolume;
    table[MidiNotes::PLAYBACK_SPEED_CC] = MidiControlChangeId::PlaybackSpeed;
    table[MidiNotes::OVERDUB_LEVEL_CC] = MidiControlChangeId::OverdubLevel;
    table[MidiNotes::EXISTING_AUDIO_LEVEL_CC] = MidiControlChangeId::ExistingAudioLevel;
    table[MidiNotes::PITCH_SHIFT_CC] = MidiControlChangeId::PitchShift;
    table[MidiNotes::METRONOME_VOLUME_CC] = MidiControlChangeId::MetronomeVolume;
    return table;
}

constexpr auto CC_MAPPING = buildCCMapping();
constexpr MidiControlChangeId getControlChangeId(uint8_t ccNumber)
{
    return ccNumber < MAX_CC_NUMBERS ? CC_MAPPING[ccNumber] : MidiControlChangeId::COUNT;
}

struct CCFlags
{
    bool needsTrackIndex : 1;
    bool isContinuous : 1;
};

constexpr CCFlags CONTROL_CHANGE_FLAGS[static_cast<size_t>(MidiControlChangeId::COUNT)] = {
    [(size_t)MidiControlChangeId::TrackSelect] = CCFlags{true, true},
    [(size_t)MidiControlChangeId::TrackVolume] = CCFlags{true, true},
    [(size_t)MidiControlChangeId::PlaybackSpeed] = CCFlags{true, true},
    [(size_t)MidiControlChangeId::OverdubLevel] = CCFlags{true, true},
    [(size_t)MidiControlChangeId::ExistingAudioLevel] = CCFlags{true, true},
    [(size_t)MidiControlChangeId::PitchShift] = CCFlags{true, true},
    [(size_t)MidiControlChangeId::MetronomeVolume] = CCFlags{false, true},
};

constexpr bool needsTrackIndex(MidiControlChangeId cc)
{
    return CONTROL_CHANGE_FLAGS[static_cast<size_t>(cc)].needsTrackIndex;
}

constexpr bool isContinuous(MidiControlChangeId cc)
{
    return CONTROL_CHANGE_FLAGS[static_cast<size_t>(cc)].isContinuous;
}

} // namespace MidiControlChangeMapping
