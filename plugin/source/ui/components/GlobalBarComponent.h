#pragma once

#include "audio/EngineStateToUIBridge.h"
#include "engine/GranularFreeze.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/FreezeComponent.h"
#include "ui/components/MetronomeComponent.h"
#include "ui/components/TransportControlsComponent.h"
#include <JuceHeader.h>

class GlobalControlBar : public juce::Component
{
public:
    GlobalControlBar (EngineMessageBus* engineMessageBus, EngineStateToUIBridge* bridge, Metronome* m, GranularFreeze* freezer)
        : uiToEngineBus (engineMessageBus)
        , transportControls (engineMessageBus, bridge)
        , metronomeComponent (engineMessageBus, m)
        , droneComponent (engineMessageBus, freezer)
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

        mainBox.items.add (juce::FlexItem (droneComponent).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));
        mainBox.items.add (juce::FlexItem().withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem (transportControls).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 0)));
        mainBox.items.add (juce::FlexItem().withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem (metronomeComponent).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 0)));

        mainBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override { g.fillAll (LooperTheme::Colors::surface); }

private:
    juce::Label looperLabel;

    EngineMessageBus* uiToEngineBus;

    TransportControlsComponent transportControls;
    MetronomeComponent metronomeComponent;
    FreezeComponent droneComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalControlBar)
};
