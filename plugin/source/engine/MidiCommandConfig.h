#pragma once
#include "audio/EngineCommandBus.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>

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
        table[i] = EngineMessageBus::CommandType::None;

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
    table[MidiNotes::METRO_TOGGLE_BUTTON] = EngineMessageBus::CommandType::ToggleMetronomeEnabled;
    table[MidiNotes::METRO_STRONG_BEAT_BUTTON] = EngineMessageBus::CommandType::SetMetronomeStrongBeat;

    return table;
}

constexpr auto NOTE_ON_COMMANDS = buildNoteOnCommands();

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

} // namespace MidiControlChangeMapping

class MidiMappingManager
{
public:
    MidiMappingManager() { loadFromJson(); }
    EngineMessageBus::CommandType getCommandForNoteOn (uint8_t note)
    {
        return note < MidiCommandMapping::MAX_MIDI_NOTES ? noteOnMapping[note] : EngineMessageBus::CommandType::None;
    }

    EngineMessageBus::CommandType getControlChangeId (uint8_t ccNumber)
    {
        return ccNumber < MidiControlChangeMapping::MAX_CC_NUMBERS ? ccMapping[ccNumber] : EngineMessageBus::CommandType::None;
    }

    void mapNoteOn (uint8_t note, EngineMessageBus::CommandType command)
    {
        if (note < MidiCommandMapping::MAX_MIDI_NOTES)
        {
            clearMappingForCommand (command);
            noteOnMapping[note] = command;
            isDirty = true;
        }
    }

    void mapControlChange (uint8_t ccNumber, EngineMessageBus::CommandType command)
    {
        if (ccNumber < MidiControlChangeMapping::MAX_CC_NUMBERS)
        {
            clearMappingForCommand (command);
            ccMapping[ccNumber] = command;
            isDirty = true;
        }
    }

    void startMidiLearn (EngineMessageBus::CommandType targetCommand)
    {
        learnState.isLearning = true;
        learnState.targetCommand = targetCommand;
        learnState.learnType = isNoteCommand (targetCommand) ? LearnType::NoteOnly
                               : isCCCommand (targetCommand) ? LearnType::CCOnly
                                                             : LearnType::Any;
    }

    void stopMidiLearn()
    {
        learnState.isLearning = false;
        learnState.targetCommand = EngineMessageBus::CommandType::None;
        learnState.learnType = LearnType::Any;
    }

    bool isLearning() const { return learnState.isLearning; }

    bool processMidiLearn (const juce::MidiMessage& msg)
    {
        if (! learnState.isLearning) return false;

        if (msg.isNoteOn() && (learnState.learnType == LearnType::Any || learnState.learnType == LearnType::NoteOnly))
        {
            uint8_t note = (uint8_t) msg.getNoteNumber();
            mapNoteOn (note, learnState.targetCommand);
            stopMidiLearn();
            return true;
        }
        else if (msg.isController() && (learnState.learnType == LearnType::Any || learnState.learnType == LearnType::CCOnly))
        {
            uint8_t ccNumber = (uint8_t) msg.getControllerNumber();
            mapControlChange (ccNumber, learnState.targetCommand);
            stopMidiLearn();
            return true;
        }
        return false;
    }

    // Save/Load
    void saveToJson()
    {
        using json = nlohmann::json;
        json j;
        j["noteOnMapping"] = noteOnMapping;
        j["ccMapping"] = ccMapping;

        auto jsonFile = getMidiMappingFile();
        jsonFile.replaceWithText (j.dump (4));
    }
    void loadFromJson()
    {
        using json = nlohmann::json;
        auto jsonFile = getMidiMappingFile();
        if (! jsonFile.existsAsFile())
        {
            resetToDefaults();
            return;
        }
        std::ifstream file (jsonFile.getFullPathName().toStdString());
        if (file.is_open())
        {
            json j;
            file >> j;
            if (j.contains ("noteOnMapping"))
                noteOnMapping = j["noteOnMapping"].get<std::array<EngineMessageBus::CommandType, MidiCommandMapping::MAX_MIDI_NOTES>>();
            if (j.contains ("ccMapping"))
                ccMapping = j["ccMapping"].get<std::array<EngineMessageBus::CommandType, MidiControlChangeMapping::MAX_CC_NUMBERS>>();
            file.close();
            isDirty = false;
        }
    }

    bool isMappingDirty() const { return isDirty; }
    void clearDirtyFlag() { isDirty = false; }

    void restoreDefaultMappings()
    {
        auto jsonFile = getMidiMappingFile();
        if (jsonFile.existsAsFile()) jsonFile.deleteFile();
        resetToDefaults();
    }

    bool isNoteCommand (EngineMessageBus::CommandType command)
    {
        for (const auto& cmd : noteOnMapping)
        {
            if (cmd == command) return true;
        }
        return false;
    }

    bool isCCCommand (EngineMessageBus::CommandType command)
    {
        for (const auto& cmd : ccMapping)
        {
            if (cmd == command) return true;
        }
        return false;
    }

    void resetToDefaults()
    {
        noteOnMapping = MidiCommandMapping::NOTE_ON_COMMANDS;
        ccMapping = MidiControlChangeMapping::CC_MAPPING;
        isDirty = false;
    }

    void clearAllMappings()
    {
        noteOnMapping.fill (EngineMessageBus::CommandType::None);
        ccMapping.fill (EngineMessageBus::CommandType::None);
        isDirty = true;
    }

private:
    void clearMappingForCommand (EngineMessageBus::CommandType command)
    {
        for (auto& cmd : noteOnMapping)
        {
            if (cmd == command) cmd = EngineMessageBus::CommandType::None;
        }
        for (auto& cmd : ccMapping)
        {
            if (cmd == command) cmd = EngineMessageBus::CommandType::None;
        }
    }

    enum class LearnType
    {
        Any,
        NoteOnly,
        CCOnly
    };

    struct LearnState
    {
        bool isLearning = false;
        EngineMessageBus::CommandType targetCommand = EngineMessageBus::CommandType::None;
        LearnType learnType = LearnType::Any;
    };

    LearnState learnState;
    bool isDirty = false;

    std::array<EngineMessageBus::CommandType, MidiCommandMapping::MAX_MIDI_NOTES> noteOnMapping = MidiCommandMapping::NOTE_ON_COMMANDS;
    std::array<EngineMessageBus::CommandType, MidiControlChangeMapping::MAX_CC_NUMBERS> ccMapping = MidiControlChangeMapping::CC_MAPPING;
    juce::File appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

    const static juce::File getMidiMappingFile()
    {
        juce::String companyName = "YourCompany";
        juce::String pluginName = "YourPlugin";
        juce::File appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

#if JUCE_WINDOWS
        // Windows: C:\Users\username\AppData\Roaming\YourCompany\YourPlugin
        appDataDir = appDataDir.getChildFile (companyName).getChildFile (pluginName);
#elif JUCE_MAC
        // macOS: ~/Library/Application Support/YourCompany/YourPlugin
        appDataDir = appDataDir.getChildFile ("Application Support").getChildFile (companyName).getChildFile (pluginName);
#elif JUCE_LINUX
        // Linux: ~/.config/YourCompany/YourPlugin
        appDataDir = appDataDir.getChildFile (".config").getChildFile (companyName).getChildFile (pluginName);
#endif
        if (! appDataDir.exists()) appDataDir.createDirectory();

        return appDataDir.getChildFile ("midi_mappings.json");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMappingManager)
};
