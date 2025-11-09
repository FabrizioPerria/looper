#pragma once
#include "audio/EngineCommandBus.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class PlaybackPitchSlider : public juce::Slider
{
public:
    PlaybackPitchSlider()
    {
        setRange (min, max, step);
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);

        setNumDecimalPlacesToDisplay (2);

        setColour (juce::Slider::textBoxTextColourId, LooperTheme::Colors::textDim);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    double snapValue (double attemptedValue, juce::Slider::DragMode) override
    {
        double value = std::round (attemptedValue * 100.0) / 100.0;
        double rounded = std::round (value);
        if (std::abs (value - rounded) < snapThreshold)
        {
            return rounded;
        }
        return value;
    }
    double getValueFromText (const juce::String& text) override { return text.getDoubleValue(); }

    juce::String getTextFromValue (double value) override { return juce::String (value, 2); }

private:
    double min = -2.0;
    double max = 2.0;
    double center = 0.0;
    double step = 0.001;
    const double snapThreshold = 0.03;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackPitchSlider)
};

class PlaybackPitchComponent : public juce::Component
{
public:
    PlaybackPitchComponent (EngineMessageBus* engineMessageBus, int trackIdx) : trackIndex (trackIdx), uiToEngineBus (engineMessageBus)
    {
        titleLabel.setText ("PITCH", juce::dontSendNotification);
        titleLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (titleLabel);

        pitchSlider.setValue (0.0);
        pitchSlider.onValueChange = [this]()
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetPlaybackPitch,
                                                                    trackIndex,
                                                                    (float) pitchSlider.getValue() });
        };
        addAndMakeVisible (pitchSlider);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int labelHeight = 12;
        titleLabel.setBounds (bounds.removeFromTop (labelHeight));
        pitchSlider.setBounds (bounds.reduced (2));
    }

    double getValue() const { return pitchSlider.getValue(); }
    void setValue (double newValue, juce::NotificationType notification) { pitchSlider.setValue (newValue, notification); }

private:
    juce::Label titleLabel;
    PlaybackPitchSlider pitchSlider;
    int trackIndex;
    EngineMessageBus* uiToEngineBus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackPitchComponent)
};
