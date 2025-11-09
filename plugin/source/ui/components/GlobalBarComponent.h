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
        , uiToEngineBus (engineMessageBus)
        , metronomeComponent (engineMessageBus, m)
        , inputMeter ("IN", engineMessageBus, bridge)
        , outputMeter ("OUT", engineMessageBus, bridge)
        , droneComponent (engineMessageBus, freezer)
    {
        addAndMakeVisible (transportControls);
        addAndMakeVisible (metronomeComponent);
        addAndMakeVisible (droneComponent);
        addAndMakeVisible (inputMeter);
        addAndMakeVisible (outputMeter);

        // Utility buttons
        midiButton.setClickingTogglesState (true);
        midiButton.onClick = [this]()
        {
            uiToEngineBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::MidiMenuEnabledChanged,
                                                                    -1,
                                                                    midiButton.getToggleState()));
        };
        midiButton.setButtonText ("MIDI");
        midiButton.setComponentID ("midi");
        addAndMakeVisible (midiButton);
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
        mainBox.items.add (juce::FlexItem (droneComponent).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));
        mainBox.items.add (juce::FlexItem (midiButton).withFlex (0.2f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        mainBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override { g.fillAll (LooperTheme::Colors::surface); }

private:
    juce::Label looperLabel;

    TransportControlsComponent transportControls;
    EngineMessageBus* uiToEngineBus;

    MetronomeComponent metronomeComponent;

    MeterWithGainComponent inputMeter;
    MeterWithGainComponent outputMeter;

    juce::TextButton midiButton { "MIDI" };

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
