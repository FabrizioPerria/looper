#pragma once

#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class LevelComponent : public juce::Component
{
public:
    LevelComponent (EngineMessageBus* engineMessageBus,
                    int trackIdx,
                    juce::String label,
                    EngineMessageBus::CommandType command,
                    float defaultValue = 0.75f,
                    float min = 0.0f,
                    float max = 2.0f)
        : uiToEngineBus (engineMessageBus), trackIndex (trackIdx), commandType (command)
    {
        knobLabel.setText (label, juce::dontSendNotification);
        knobLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        knobLabel.setJustificationType (juce::Justification::centred);
        knobLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (knobLabel);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRange (min, max, 0.01);
        slider.setValue (defaultValue);
        slider.onValueChange = [this]()
        {
            if (uiToEngineBus)
                uiToEngineBus->pushCommand (EngineMessageBus::Command { commandType, trackIndex, (float) slider.getValue() });
        };
        addAndMakeVisible (slider);

        slider.setInterceptsMouseClicks (false, false);
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

    std::function<void (const juce::MouseEvent&)> onShiftClick = nullptr;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isShiftDown() && onShiftClick)
        {
            onShiftClick (e);
            return;
        }

        slider.mouseDown (e.getEventRelativeTo (&slider));
    }

    void mouseDrag (const juce::MouseEvent& e) override { slider.mouseDrag (e.getEventRelativeTo (&slider)); }

    void mouseUp (const juce::MouseEvent& e) override { slider.mouseUp (e.getEventRelativeTo (&slider)); }

private:
    std::function<void (const juce::MouseEvent&)> mouseCallback;

    EngineMessageBus* uiToEngineBus = nullptr;
    int trackIndex;
    EngineMessageBus::CommandType commandType;

    juce::Label knobLabel;
    juce::Slider slider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelComponent)
};
