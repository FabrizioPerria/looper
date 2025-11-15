#pragma once
#include "audio/EngineCommandBus.h"
#include "ui/components/LevelComponent.h"
#include <JuceHeader.h>

class VolumesComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    VolumesComponent (EngineMessageBus* engineMessageBus, int trackIdx)
        : uiToEngineBus (engineMessageBus)
        , trackIndex (trackIdx)
        , overdubLevelKnob (engineMessageBus,
                            trackIndex,
                            "BASE LEVEL",
                            EngineMessageBus::CommandType::SetNewOverdubGain,
                            OVERDUB_DEFAULT_GAIN,
                            MIN_OVERDUB_GAIN,
                            MAX_OVERDUB_GAIN)
        , existingAudioLevelKnob (engineMessageBus,
                                  trackIndex,
                                  "NEW LEVEL",
                                  EngineMessageBus::CommandType::SetExistingAudioGain,
                                  BASE_DEFAULT_GAIN,
                                  MIN_BASE_GAIN,
                                  MAX_BASE_GAIN)
    {
        // normalizeButton.setButtonText ("NORM");
        // normalizeButton.setComponentID ("normalize");
        // normalizeButton.setClickingTogglesState (true);
        // normalizeButton.onClick = [this]()
        // {
        //     uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleVolumeNormalize, trackIndex, {} });
        // };
        // addAndMakeVisible (normalizeButton);

        addAndMakeVisible (overdubLevelKnob);
        addAndMakeVisible (existingAudioLevelKnob);

        uiToEngineBus->addListener (this);
    }

    ~VolumesComponent() override { uiToEngineBus->removeListener (this); }

    void resized() override
    {
        auto bounds = getLocalBounds();

        juce::FlexBox mainRow;
        mainRow.flexDirection = juce::FlexBox::Direction::row;
        mainRow.alignItems = juce::FlexBox::AlignItems::stretch;

        juce::FlexBox existingFlex;
        existingFlex.flexDirection = juce::FlexBox::Direction::column;
        existingFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        existingFlex.items.add (juce::FlexItem (existingAudioLevelKnob).withFlex (1.0f));

        mainRow.items.add (juce::FlexItem (existingFlex).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));

        // mainRow.items.add (juce::FlexItem (normalizeButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));

        juce::FlexBox overdubFlex;
        overdubFlex.flexDirection = juce::FlexBox::Direction::column;
        overdubFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        overdubFlex.items.add (juce::FlexItem (overdubLevelKnob).withFlex (1.0f));

        mainRow.items.add (juce::FlexItem (overdubFlex).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        mainRow.performLayout (bounds.toFloat());
    }

private:
    EngineMessageBus* uiToEngineBus = nullptr;
    int trackIndex;

    // juce::TextButton normalizeButton;
    LevelComponent overdubLevelKnob;
    LevelComponent existingAudioLevelKnob;

    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::OldOverdubGainLevels,
                                                                        EngineMessageBus::EventType::NewOverdubGainLevels };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        if (event.trackIndex != trackIndex) return;

        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            // case EngineMessageBus::EventType::NormalizeStateChanged:
            //     if (std::holds_alternative<bool> (event.data))
            //     {
            //         auto isNormalized = std::get<bool> (event.data);
            //         normalizeButton.setToggleState (isNormalized, juce::dontSendNotification);
            //     }
            //     break;
            case EngineMessageBus::EventType::OldOverdubGainLevels:
                if (std::holds_alternative<float> (event.data))
                {
                    auto oldGain = std::get<float> (event.data);

                    if (std::abs (existingAudioLevelKnob.getValue() - oldGain) > 0.01)
                        existingAudioLevelKnob.setValue (oldGain, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::NewOverdubGainLevels:
                if (std::holds_alternative<float> (event.data))
                {
                    auto newGain = std::get<float> (event.data);

                    if (std::abs (overdubLevelKnob.getValue() - newGain) > 0.01)
                        overdubLevelKnob.setValue (newGain, juce::dontSendNotification);
                }
                break;
            default:
                throw juce::String ("Unhandled event type in VolumesComponent: ") + juce::String ((int) event.type);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumesComponent)
};
