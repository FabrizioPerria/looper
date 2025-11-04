#pragma once

#include "audio/EngineCommandBus.h"
#include <JuceHeader.h>

class FreezeComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    FreezeComponent (EngineMessageBus* engineMessageBus) : uiToEngineBus (engineMessageBus)
    {
        freezeButton.setButtonText ("FREEZE");
        freezeButton.setComponentID ("freeze");
        freezeButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleFreeze, -1, {} }); };
        addAndMakeVisible (freezeButton);

        uiToEngineBus->addListener (this);
    }
    ~FreezeComponent() override { uiToEngineBus->removeListener (this); }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4);
        freezeButton.setBounds (bounds);
    }

private:
    juce::TextButton freezeButton;
    EngineMessageBus* uiToEngineBus;

    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::FreezeStateChanged };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::FreezeStateChanged:
            {
                bool isFrozen = std::get<bool> (event.data);
                freezeButton.setToggleState (isFrozen, juce::dontSendNotification);
                break;
            }
            default:
                throw juce::String ("Unhandled event type in FreezeComponent" + juce::String (static_cast<int> (event.type)));
        }
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeComponent);
};
