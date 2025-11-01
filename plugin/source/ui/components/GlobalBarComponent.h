#pragma once

#include "audio/EngineStateToUIBridge.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/MetronomeComponent.h"
#include "ui/components/TransportControlsComponent.h"
#include <JuceHeader.h>

class GlobalControlBar : public juce::Component
{
public:
    GlobalControlBar (EngineMessageBus* engineMessageBus, EngineStateToUIBridge* bridge)
        : transportControls (engineMessageBus, bridge), metronomeComponent (engineMessageBus)
    {
        looperLabel.setText ("LOOPER", juce::NotificationType::dontSendNotification);
        juce::FontOptions fontOptions = juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold);

        looperLabel.setFont (juce::Font (fontOptions));
        looperLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (looperLabel);

        addAndMakeVisible (transportControls);
        addAndMakeVisible (metronomeComponent);

        // Utility buttons
        setupButton (saveButton);
        setupButton (droneButton);
    }

    ~GlobalControlBar() override {}

    void resized() override
    {
        auto bounds = getLocalBounds();
        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::row;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;

        // Logo
        mainBox.items.add (juce::FlexItem (looperLabel).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 2, 0, 2)));

        mainBox.items.add (juce::FlexItem (transportControls).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));

        // Utility buttons
        mainBox.items.add (juce::FlexItem (metronomeComponent).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));
        mainBox.items.add (juce::FlexItem (saveButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem (droneButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));

        mainBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override { g.fillAll (LooperTheme::Colors::surface); }

private:
    juce::Label looperLabel;

    TransportControlsComponent transportControls;

    MetronomeComponent metronomeComponent;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton droneButton { "DRONE" };

    void setupButton (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, juce::Colour (60, 60, 60));
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colour (100, 150, 200));
        button.setColour (juce::TextButton::textColourOffId, juce::Colour (200, 200, 200));
        button.setColour (juce::TextButton::textColourOnId, juce::Colour (255, 255, 255));
        addAndMakeVisible (button);
    }

    void setupSlider (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 50, 20);
        slider.setRange (0.0, 1.0, 0.01);
        slider.setValue (0.5);
        slider.setColour (juce::Slider::trackColourId, LooperTheme::Colors::primary);
        slider.setColour (juce::Slider::thumbColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (slider);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalControlBar)
};
