#pragma once
#include "engine/LooperEngine.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/GlobalBarComponent.h"
#include "ui/components/MidiMappingComponent.h"
#include "ui/components/TrackComponent.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class LooperEditor : public juce::Component
{
public:
    LooperEditor (LooperEngine* engine) : looperEngine (engine)
    {
        globalBar = std::make_unique<GlobalControlBar> (engine->getMessageBus(),
                                                        engine->getEngineStateBridge(),
                                                        engine->getMetronome(),
                                                        engine->getGranularFreeze());

        midiMappingComponent = std::make_unique<MidiMappingComponent> (engine->getMidiMappingManager(), engine->getMessageBus());

        addAndMakeVisible (*midiMappingComponent);

        for (int i = 0; i < engine->getNumTracks(); ++i)
        {
            auto* channel = new TrackComponent (engine->getMessageBus(),
                                                i,
                                                engine->getTrackByIndex (i)->getUIBridge(),
                                                engine->getEngineStateBridge());
            channels.add (channel);
            addAndMakeVisible (channel);
        }

        addAndMakeVisible (*globalBar);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (LooperTheme::Colors::backgroundDark);
        g.setColour (juce::Colours::red);
        g.drawRect (globalBar->getBounds(), 2.0f); // Draw red box around globalBar
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        juce::FlexBox mainFlex;
        mainFlex.flexDirection = juce::FlexBox::Direction::column;
        mainFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        mainFlex.items.add (juce::FlexItem (*globalBar).withFlex (0.25f));

        for (auto* channel : channels)
        {
            mainFlex.items.add (juce::FlexItem().withFlex (0.05f)); // spacer between tracks
            mainFlex.items.add (juce::FlexItem (*channel).withFlex (0.8f));
        }

        mainFlex.performLayout (bounds.toFloat());
    }

private:
    LooperEngine* looperEngine;

    std::unique_ptr<GlobalControlBar> globalBar;
    juce::OwnedArray<TrackComponent> channels;
    std::unique_ptr<MidiMappingComponent> midiMappingComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEditor)
};
