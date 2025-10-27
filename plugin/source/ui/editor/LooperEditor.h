#pragma once
#include "ui/colors/TokyoNight.h"
#include "ui/components/GlobalBarComponent.h"
#include "ui/components/TrackComponent.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class LooperEditor : public juce::Component, public juce::Timer
{
public:
    LooperEditor (LooperEngine* engine) : looperEngine (engine)
    {
        midiDispatcher = std::make_unique<MidiCommandDispatcher> (engine);
        globalBar = std::make_unique<GlobalControlBar> (midiDispatcher.get(), engine->getEngineStateBridge());

        for (int i = 0; i < engine->getNumTracks(); ++i)
        {
            auto* channel = new TrackComponent (midiDispatcher.get(),
                                                i,
                                                engine->getUIBridgeByIndex (i),
                                                engine->getEngineStateBridge(),
                                                engine->getUIToEngineBridge());
            channels.add (channel);
            addAndMakeVisible (channel);
        }

        addAndMakeVisible (*globalBar);

        startTimerHz (10);
    }

    ~LooperEditor() override { stopTimer(); }

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

        mainFlex.items.add (juce::FlexItem (*globalBar).withFlex (0.2f));

        for (auto* channel : channels)
        {
            mainFlex.items.add (juce::FlexItem().withFlex (0.05f)); // spacer between tracks
            mainFlex.items.add (juce::FlexItem (*channel).withFlex (0.8f));
        }

        mainFlex.performLayout (bounds.toFloat());
    }

private:
    LooperEngine* looperEngine;
    std::unique_ptr<MidiCommandDispatcher> midiDispatcher;

    std::unique_ptr<GlobalControlBar> globalBar;
    juce::OwnedArray<TrackComponent> channels;

    void timerCallback() override
    {
        // Update active track highlighting
        int activeTrackIndex = looperEngine->getActiveTrackIndex();
        for (int i = 0; i < channels.size(); ++i)
        {
            channels[i]->setActive (i == activeTrackIndex);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEditor)
};
