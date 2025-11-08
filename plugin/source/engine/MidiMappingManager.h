#pragma once

#include <JuceHeader.h>

class MidiMappingManager
{
    // Allow remapping
    void mapNoteOn (uint8_t note, MidiCommandId command);

    // MIDI learn
    void startMidiLearn (MidiCommandId targetCommand);
    bool processMidiLearn (const juce::MidiMessage& msg);

    // Save/Load
    void saveToXml (juce::XmlElement& xml);
    void loadFromXml (const juce::XmlElement& xml);
};
