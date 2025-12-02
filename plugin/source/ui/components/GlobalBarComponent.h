#pragma once

#include "ui/colors/TokyoNight.h"
#include "ui/components/FreezeComponent.h"
#include "ui/components/MetronomeComponent.h"
#include "ui/components/TransportControlsComponent.h"
#include <JuceHeader.h>

class GlobalControlBar : public juce::Component
{
public:
    GlobalControlBar (EngineMessageBus* engineMessageBus, Metronome* m, AutomationEngine* automationEngine)
        : transportControls (engineMessageBus)
        , metronomeComponent (engineMessageBus, m, automationEngine)
        , droneComponent (engineMessageBus)
    {
        addAndMakeVisible (transportControls);
        addAndMakeVisible (metronomeComponent);
        addAndMakeVisible (droneComponent);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::row;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;

        mainBox.items.add (juce::FlexItem (droneComponent).withFlex (0.2f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));
        mainBox.items.add (juce::FlexItem().withFlex (0.1f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem (transportControls).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem().withFlex (0.1f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem (metronomeComponent).withFlex (0.65f).withMargin (juce::FlexItem::Margin (0, 0, 1, 0)));

        mainBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override { g.fillAll (LooperTheme::Colors::surface); }

private:
    juce::Label looperLabel;

    TransportControlsComponent transportControls;
    MetronomeComponent metronomeComponent;
    FreezeComponent droneComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalControlBar)
};
