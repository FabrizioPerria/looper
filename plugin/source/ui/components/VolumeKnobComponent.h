#pragma once

#include "ui/colors/TokyoNight.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class VolumeKnobComponent : public juce::Component
{
public:
    VolumeKnobComponent (MidiCommandDispatcher* dispatcher, int trackIdx) : midiDispatcher (dispatcher), trackIndex (trackIdx)
    {
        volumeLabel.setText ("VOLUME", juce::dontSendNotification);
        volumeLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        volumeLabel.setJustificationType (juce::Justification::centred);
        volumeLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (volumeLabel);

        volumeFader.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        volumeFader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        volumeFader.setRange (0.0, 1.0, 0.01);
        volumeFader.setValue (0.75);
        volumeFader.onValueChange = [this]()
        { midiDispatcher->sendControlChangeToEngine (MidiNotes::TRACK_VOLUME_CC, trackIndex, volumeFader.getValue()); };
        addAndMakeVisible (volumeFader);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int labelHeight = 12;
        volumeLabel.setBounds (bounds.removeFromTop (labelHeight));
        volumeFader.setBounds (bounds.reduced (2));
    }

    void setValue (double newValue, juce::NotificationType notification = juce::sendNotification)
    {
        volumeFader.setValue (newValue, notification);
    }

    float getValue() const { return (float) volumeFader.getValue(); }

private:
    MidiCommandDispatcher* midiDispatcher;
    int trackIndex;

    juce::Label volumeLabel;
    juce::Slider volumeFader;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumeKnobComponent)
};
