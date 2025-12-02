#pragma once
#include "engine/Constants.h"
#include "engine/LooperEngine.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/FooterComponent.h"
#include "ui/components/GlobalBarComponent.h"
#include "ui/components/MidiMappingComponent.h"
#include "ui/components/TrackComponent.h"
#include <JuceHeader.h>

class ProgressiveSpeedPopup;

class LooperEditor : public juce::Component
{
public:
    LooperEditor (LooperEngine* engine)
    {
        globalBar = std::make_unique<GlobalControlBar> (engine->getMessageBus(), engine->getMetronome());

        footerComponent = std::make_unique<FooterComponent> (engine->getMessageBus(), engine->getEngineStateBridge(), engine);
        midiMappingComponent = std::make_unique<MidiMappingComponent> (engine->getMidiMappingManager(), engine->getMessageBus());

        for (int i = 0; i < engine->getNumTracks(); ++i)
        {
            auto channel = std::make_unique<TrackComponent> (engine->getMessageBus(),
                                                             i,
                                                             engine->getTrackByIndex (i)->getUIBridge(),
                                                             engine->getAutomationEngine());
            channels[(size_t) i] = std::move (channel);
            addAndMakeVisible (*channels[(size_t) i]);
        }

        addAndMakeVisible (*globalBar);
        addAndMakeVisible (*midiMappingComponent);
        addAndMakeVisible (*footerComponent);
        midiMappingComponent->setVisible (false);
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

        mainFlex.items.add (juce::FlexItem (*globalBar).withFlex (0.3f));

        for (int i = 0; i < NUM_TRACKS; ++i)
        {
            auto* channel = channels[(size_t) i].get();

            mainFlex.items.add (juce::FlexItem().withFlex (0.05f)); // spacer between tracks
            mainFlex.items.add (juce::FlexItem (*channel).withFlex (0.8f));
        }
        mainFlex.items.add (juce::FlexItem().withFlex (0.05f)); // spacer between tracks

        mainFlex.items.add (juce::FlexItem (*footerComponent).withFlex (0.25f));

        mainFlex.performLayout (bounds.toFloat());

        // the idea is to preallocate an overlay area for the midiMappingComponent. When it's visible, it will cover the right half of the editor.
        // When it's hidden, it will be set to zero size internally in the midiMappingComponent's resized() method.
        auto midiMappingArea = getLocalBounds().withTrimmedLeft (getWidth() / 2 - 60);
        midiMappingArea.removeFromTop (globalBar->getHeight());
        midiMappingArea.removeFromBottom (footerComponent->getHeight());
        midiMappingComponent->setBounds (midiMappingArea);
    }

private:
    std::unique_ptr<GlobalControlBar> globalBar;
    std::array<std::unique_ptr<TrackComponent>, NUM_TRACKS> channels;
    std::unique_ptr<FooterComponent> footerComponent;
    std::unique_ptr<MidiMappingComponent> midiMappingComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEditor)
};
