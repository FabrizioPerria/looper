#pragma once
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
        setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 15);

        // Format the text to show semitones and cents
        // setTextValueSuffix (" st");
        setNumDecimalPlacesToDisplay (2);

        // Style the text box
        setColour (juce::Slider::textBoxTextColourId, LooperTheme::Colors::textDim);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

        // Use the theme font
        // setFont (LooperTheme::Fonts::getRegularFont (11.0f));
    }

    double snapValue (double attemptedValue, juce::Slider::DragMode) override
    {
        // Round to 2 decimal places first to avoid floating point imprecision
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
    double step = 0.001;               // 0.01 = 1 cent
    const double snapThreshold = 0.03; // Snap within 3 cents of a semitone

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackPitchSlider)
};

class PlaybackPitchComponent : public juce::Component
{
public:
    PlaybackPitchComponent (MidiCommandDispatcher* dispatcher, int trackIdx) : trackIndex (trackIdx), midiDispatcher (dispatcher)
    {
        titleLabel.setText ("PITCH", juce::dontSendNotification);
        titleLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (titleLabel);

        pitchSlider.setValue (0.0);
        pitchSlider.onValueChange = [this]()
        {
            // // Convert float semitones to 0-127 MIDI range
            // float semitones = (float) pitchSlider.getValue(); // -2.0 to +2.0
            // int midiValue = juce::jmap (semitones, -2.0f, 2.0f, 0.0f, 127.0f);
            midiDispatcher->sendControlChangeToEngine (MidiNotes::PITCH_SHIFT_CC, trackIndex, pitchSlider.getValue());
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
    MidiCommandDispatcher* midiDispatcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackPitchComponent)
};
