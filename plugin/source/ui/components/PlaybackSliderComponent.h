#pragma once
#include <JuceHeader.h>

class PlaybackSpeedSlider : public juce::Slider
{
public:
    PlaybackSpeedSlider()
    {
        setRange (min, max, step);
        setSliderStyle (juce::Slider::LinearHorizontal);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    }

    double snapValue (double attemptedValue, juce::Slider::DragMode) override
    {
        const double snapThreshold = 0.03;

        if (std::abs (attemptedValue - 0.5) < snapThreshold) return 0.5;
        if (std::abs (attemptedValue - 0.75) < snapThreshold) return 0.75;
        if (std::abs (attemptedValue - 1.0) < snapThreshold) return 1.0;
        if (std::abs (attemptedValue - 1.5) < snapThreshold) return 1.5;
        if (std::abs (attemptedValue - 2.0) < snapThreshold) return 2.0;

        return attemptedValue;
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

    double min = 0.5;
    double max = 2.0;
    double center = 1.0;
    double step = 0.01;
};
