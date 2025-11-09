#pragma once
#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class PlaybackSpeedSlider : public juce::Slider
{
public:
    PlaybackSpeedSlider()
    {
        setRange (min, max, step);
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    }

    double snapValue (double attemptedValue, juce::Slider::DragMode) override
    {
        if (std::abs (attemptedValue - 0.5) < snapThreshold) return 0.5;
        if (std::abs (attemptedValue - 0.75) < snapThreshold) return 0.75;
        if (std::abs (attemptedValue - 1.0) < snapThreshold) return 1.0;
        if (std::abs (attemptedValue - 1.5) < snapThreshold) return 1.5;
        if (std::abs (attemptedValue - 2.0) < snapThreshold) return 2.0;

        return attemptedValue;
    }

    bool isInSnapRange (double value)
    {
        return (std::abs (value - 0.5) < snapThreshold) || (std::abs (value - 0.75) < snapThreshold)
               || (std::abs (value - 1.0) < snapThreshold) || (std::abs (value - 1.5) < snapThreshold)
               || (std::abs (value - 2.0) < snapThreshold);
    }

    double getValueFromText (const juce::String& text) override { return text.getDoubleValue(); }

    juce::String getTextFromValue (double value) override { return juce::String (value, 2) + "x"; }

    // Custom mapping: linear from 0.5→1.0 (left half) and 1.0→2.0 (right half)
    double valueToProportionOfLength (double value) override
    {
        if (value <= center)
        {
            return juce::jmap (value, min, center, 0.0, 0.5);
        }
        else
        {
            return juce::jmap (value, center, max, 0.5, 1.0);
        }
    }

    double proportionOfLengthToValue (double proportion) override
    {
        if (proportion <= 0.5)
        {
            return juce::jmap (proportion, 0.0, 0.5, min, center);
        }
        else
        {
            return juce::jmap (proportion, 0.5, 1.0, center, max);
        }
    }

private:
    double min = MIN_PLAYBACK_SPEED;
    double max = MAX_PLAYBACK_SPEED;
    double center = 1.0;
    double step = 0.01;
    const double snapThreshold = 0.03;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackSpeedSlider)
};

class PlaybackSpeedComponent : public juce::Component
{
public:
    PlaybackSpeedComponent (EngineMessageBus* engineMessageBus, int trackIdx) : trackIndex (trackIdx), uiToEngineBus (engineMessageBus)
    {
        titleLabel.setText ("SPEED", juce::dontSendNotification);
        titleLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (titleLabel);

        speedSlider.setValue (DEFAULT_PLAYBACK_SPEED);
        speedSlider.onValueChange = [this]()
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetPlaybackSpeed,
                                                                    trackIndex,
                                                                    (float) speedSlider.getValue() });
        };
        addAndMakeVisible (speedSlider);
    }

    void setValue (double newValue, juce::NotificationType notification) { speedSlider.setValue (newValue, notification); }

    double getValue() const { return speedSlider.getValue(); }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int labelHeight = 12;
        titleLabel.setBounds (bounds.removeFromTop (labelHeight));
        speedSlider.setBounds (bounds.reduced (2));
    }

private:
    juce::Label titleLabel;
    PlaybackSpeedSlider speedSlider;
    int trackIndex;
    EngineMessageBus* uiToEngineBus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackSpeedComponent)
};
