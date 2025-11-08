#pragma once

#include "audio/EngineStateToUIBridge.h"
#include "engine/GranularFreeze.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/FreezeComponent.h"
#include "ui/components/MeterWithGainComponent.h"
#include "ui/components/MetronomeComponent.h"
#include "ui/components/TransportControlsComponent.h"
#include <JuceHeader.h>

class GlobalControlBar : public juce::Component
{
public:
    GlobalControlBar (EngineMessageBus* engineMessageBus, EngineStateToUIBridge* bridge, Metronome* m, GranularFreeze* freezer)
        : transportControls (engineMessageBus, bridge)
        , metronomeComponent (engineMessageBus, m)
        , inputMeter ("IN", engineMessageBus, bridge)
        , outputMeter ("OUT", engineMessageBus, bridge)
        , droneComponent (engineMessageBus, freezer)
    {
        looperLabel.setText ("LOOPER", juce::NotificationType::dontSendNotification);
        juce::FontOptions fontOptions = juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold);

        looperLabel.setFont (juce::Font (fontOptions));
        looperLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (looperLabel);

        addAndMakeVisible (transportControls);
        addAndMakeVisible (metronomeComponent);
        addAndMakeVisible (droneComponent);
        addAndMakeVisible (inputMeter);
        addAndMakeVisible (outputMeter);

        // Utility buttons
        saveButton.onClick = [engineMessageBus]()
        { engineMessageBus->pushCommand ({ EngineMessageBus::CommandType::saveMidiMappings, -1, std::monostate {} }); };
        setupButton (saveButton);
    }

    ~GlobalControlBar() override {}

    void resized() override
    {
        auto bounds = getLocalBounds();
        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::row;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;

        mainBox.items.add (juce::FlexItem (inputMeter).withFlex (0.6f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));
        mainBox.items.add (juce::FlexItem (outputMeter).withFlex (0.6f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));

        mainBox.items.add (juce::FlexItem (transportControls).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 0)));

        // Utility buttons
        mainBox.items.add (juce::FlexItem (metronomeComponent).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 0)));
        mainBox.items.add (juce::FlexItem (saveButton).withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem (droneComponent).withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));

        mainBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override { g.fillAll (LooperTheme::Colors::surface); }

private:
    juce::Label looperLabel;

    TransportControlsComponent transportControls;

    MetronomeComponent metronomeComponent;

    MeterWithGainComponent inputMeter;
    MeterWithGainComponent outputMeter;

    juce::TextButton saveButton { "SAVE" };
    FreezeComponent droneComponent;

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
