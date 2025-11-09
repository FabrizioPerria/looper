#pragma once

#include "audio/EngineCommandBus.h"
#include "engine/GranularFreeze.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/ButtonIconComponent.h"
#include "ui/components/LevelComponent.h"
#include <JuceHeader.h>

class FreezeComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    FreezeComponent (EngineMessageBus* engineMessageBus, GranularFreeze* freezer)
        : uiToEngineBus (engineMessageBus)
        , freezeSynth (freezer)
        , levelComponent (engineMessageBus, -1, "Level", EngineMessageBus::CommandType::SetFreezeLevel)
    // , freezeButton (engineMessageBus, BinaryData::freeze_svg, EngineMessageBus::CommandType::ToggleFreeze)
    {
        freezeLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        freezeLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (freezeLabel);
        freezeButton.setButtonText ("Enable");
        freezeButton.setColour (juce::TextButton::buttonColourId, LooperTheme::Colors::surface);
        freezeButton.setColour (juce::TextButton::buttonOnColourId, LooperTheme::Colors::green);
        freezeButton.setColour (juce::TextButton::textColourOffId, LooperTheme::Colors::textDim);
        freezeButton.setColour (juce::TextButton::textColourOnId, LooperTheme::Colors::background);
        freezeButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleFreeze, -1, std::monostate {} }); };

        addAndMakeVisible (freezeButton);
        addAndMakeVisible (levelComponent);
        uiToEngineBus->addListener (this);
    }

    ~FreezeComponent() override { uiToEngineBus->removeListener (this); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));

        auto titleBounds = freezeLabel.getBounds();
        g.fillRect (titleBounds.getX() + 3.0f, titleBounds.getBottom() + 3.0f, titleBounds.getWidth() - 6.0f, 1.0f);
    }

    void resized() override
    {
        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::column;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;

        mainBox.items.add (juce::FlexItem (freezeLabel).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        juce::FlexBox layoutBox;
        layoutBox.flexDirection = juce::FlexBox::Direction::row;
        layoutBox.alignItems = juce::FlexBox::AlignItems::stretch;
        layoutBox.items.add (juce::FlexItem (freezeButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (2, 1, 0, 1)));
        layoutBox.items.add (juce::FlexItem (levelComponent).withFlex (0.5f).withMargin (juce::FlexItem::Margin (2, 1, 0, 1)));

        mainBox.items.add (juce::FlexItem (layoutBox).withFlex (3.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        mainBox.performLayout (getLocalBounds().toFloat());
    }

private:
    EngineMessageBus* uiToEngineBus;
    GranularFreeze* freezeSynth;
    juce::Label freezeLabel { "Freeze", "Freeze" };
    LevelComponent levelComponent;
    // ButtonIconComponent freezeButton;
    juce::TextButton freezeButton;

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
                // freezeButton.setFreezeEnabled (isFrozen);
                break;
            }
            default:
                throw juce::String ("Unhandled event type in FreezeComponent" + juce::String (static_cast<int> (event.type)));
        }
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeComponent);
};
