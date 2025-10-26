#pragma once

#include "ui/colors/TokyoNight.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class LevelComponent : public juce::Component
{
public:
    LevelComponent (MidiCommandDispatcher* dispatcher, int trackIdx, juce::String label, int cc)
        : midiDispatcher (dispatcher), trackIndex (trackIdx)
    {
        knobLabel.setText (label, juce::dontSendNotification);
        knobLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        knobLabel.setJustificationType (juce::Justification::centred);
        knobLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (knobLabel);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRange (0.0, 1.0, 0.01);
        slider.setValue (0.75);
        slider.onValueChange = [this, cc]() { midiDispatcher->sendControlChangeToEngine (cc, trackIndex, slider.getValue()); };
        addAndMakeVisible (slider);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int labelHeight = 12;
        knobLabel.setBounds (bounds.removeFromTop (labelHeight));
        slider.setBounds (bounds.reduced (2));
    }

    void setValue (double newValue, juce::NotificationType notification = juce::sendNotification)
    {
        slider.setValue (newValue, notification);
    }

    float getValue() const { return (float) slider.getValue(); }

private:
    MidiCommandDispatcher* midiDispatcher;
    int trackIndex;

    juce::Label knobLabel;
    juce::Slider slider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelComponent)
};
