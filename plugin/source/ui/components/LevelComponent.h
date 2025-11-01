#pragma once

#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class LevelComponent : public juce::Component
{
public:
    LevelComponent (EngineMessageBus* engineMessageBus, int trackIdx, juce::String label, int cc)
        : uiToEngineBus (engineMessageBus), trackIndex (trackIdx)
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
        slider.onValueChange = [this, cc]()
        {
            auto ccValue = (int) std::clamp (slider.getValue() * 127.0, 0.0, 127.0);
            juce::MidiBuffer midiBuffer;
            juce::MidiMessage msg = juce::MidiMessage::controllerEvent (1, (juce::uint8) cc, (juce::uint8) ccValue);
            midiBuffer.addEvent (msg, 0);

            if (uiToEngineBus)
                uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::MidiMessage,
                                                                        trackIndex,
                                                                        midiBuffer });
        };
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
    EngineMessageBus* uiToEngineBus = nullptr;
    int trackIndex;

    juce::Label knobLabel;
    juce::Slider slider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelComponent)
};
