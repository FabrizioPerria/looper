#pragma once
#include "engine/LooperEngine.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

// 1 checkbox button to enable/disable volume normalization
// if normalization is disabled. show 2 small knobs for overdub levels and existing audio levels
class VolumesComponent : public juce::Component
{
public:
    VolumesComponent (LooperEngine* engine, int trackIdx, AudioToUIBridge* bridge) : looperEngine (engine), trackIndex (trackIdx)
    {
        normalizeButton.setButtonText ("Normalize Volumes");
        normalizeButton.setClickingTogglesState (true);
        normalizeButton.onClick = [this]()
        {
            juce::MidiBuffer midiBuffer;
            midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
            midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1,
                                                                     MidiNotes::VOLUME_NORMALIZE_BUTTON,
                                                                     normalizeButton.getToggleState() ? 127 : 0),
                                 0);
            looperEngine->handleMidiCommand (midiBuffer);
        };
        addAndMakeVisible (normalizeButton);

        overdubLevelKnob.setSliderStyle (juce::Slider::Rotary);
        overdubLevelKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        overdubLevelKnob.setRange (0.0, 1.0, 0.01);
        overdubLevelKnob.setValue (0.5);
        overdubLevelKnob.onValueChange = [this]()
        {
            juce::MidiBuffer midiBuffer;
            midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
            int ccValue = (int) std::clamp (overdubLevelKnob.getValue() * 127.0, 0.0, 127.0);
            midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::OVERDUB_LEVEL_CC, ccValue), 0);
            looperEngine->handleMidiCommand (midiBuffer);
        };
        addAndMakeVisible (overdubLevelKnob);

        existingAudioLevelKnob.setSliderStyle (juce::Slider::Rotary);
        existingAudioLevelKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        existingAudioLevelKnob.setRange (0.0, 1.0, 0.01);
        existingAudioLevelKnob.setValue (0.5);
        existingAudioLevelKnob.onValueChange = [this]()
        {
            juce::MidiBuffer midiBuffer;
            midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
            int ccValue = (int) std::clamp (existingAudioLevelKnob.getValue() * 127.0, 0.0, 127.0);
            midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::EXISTING_AUDIO_LEVEL_CC, ccValue), 0);
            looperEngine->handleMidiCommand (midiBuffer);
        };
        addAndMakeVisible (existingAudioLevelKnob);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4);
        bounds.removeFromLeft (2);

        juce::FlexBox mainRow;
        mainRow.flexDirection = juce::FlexBox::Direction::column;
        mainRow.alignItems = juce::FlexBox::AlignItems::stretch;

        mainRow.items.add (juce::FlexItem (normalizeButton).withFlex (0.3f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));
        mainRow.items.add (juce::FlexItem (overdubLevelKnob).withFlex (0.35f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));
        mainRow.items.add (juce::FlexItem (existingAudioLevelKnob).withFlex (0.35f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        mainRow.performLayout (bounds.toFloat());
    }

private:
    int trackIndex;

    juce::ToggleButton normalizeButton;
    juce::Slider overdubLevelKnob;
    juce::Slider existingAudioLevelKnob;

    LooperEngine* looperEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumesComponent)
};
