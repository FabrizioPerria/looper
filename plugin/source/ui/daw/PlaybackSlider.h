#pragma once
#include <JuceHeader.h>

class PlaybackSpeedSlider : public juce::Slider
{
public:
    PlaybackSpeedSlider()
    {
        setRange (0.2, 2.0, 0.01);
        setSkewFactorFromMidPoint (1.0); // Makes 1.0x appear in the middle
        setSliderStyle (juce::Slider::LinearHorizontal);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    }

    // Snap to key values when dragging
    double snapValue (double attemptedValue, juce::Slider::DragMode) override
    {
        const double snapThreshold = 0.05;

        if (std::abs (attemptedValue - 0.5) < snapThreshold) return 0.5;
        if (std::abs (attemptedValue - 1.0) < snapThreshold) return 1.0;
        if (std::abs (attemptedValue - 2.0) < snapThreshold) return 2.0;

        return attemptedValue;
    }
};
