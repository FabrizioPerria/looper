#pragma once

#include "audio/EngineCommandBus.h"
#include "audio/EngineStateToUIBridge.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class TransportControlsComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    TransportControlsComponent (EngineMessageBus* engineMessageBus, EngineStateToUIBridge* bridge)
        : uiToEngineBus (engineMessageBus), engineState (bridge)
    {
        recButton.setButtonText ("REC");
        recButton.setComponentID ("rec");
        recButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleRecord, -1, {} }); };
        addAndMakeVisible (recButton);

        playButton.setButtonText ("PLAY");
        playButton.setComponentID ("play");
        playButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::TogglePlay, -1, {} }); };
        addAndMakeVisible (playButton);

        playModeButton.setButtonText ("SINGLE");
        playModeButton.setComponentID ("single");
        playModeButton.setToggleState (true, juce::dontSendNotification);
        playModeButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleSinglePlayMode, -1, {} }); };
        addAndMakeVisible (playModeButton);

        stopButton.setButtonText ("STOP");
        stopButton.setComponentID ("stop");
        stopButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::Stop, -1, {} }); };
        addAndMakeVisible (stopButton);

        prevButton.setButtonText ("PREV");
        prevButton.setComponentID ("prev");
        prevButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::PreviousTrack, -1, {} }); };
        addAndMakeVisible (prevButton);

        nextButton.setButtonText ("NEXT");
        nextButton.setComponentID ("next");
        nextButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::NextTrack, -1, {} }); };
        addAndMakeVisible (nextButton);

        uiToEngineBus->addListener (this);
    }

    ~TransportControlsComponent() override { uiToEngineBus->removeListener (this); }

    void resized() override
    {
        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::row;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;

        juce::FlexBox playStopBox;
        playStopBox.flexDirection = juce::FlexBox::Direction::column;
        playStopBox.items.add (juce::FlexItem (playButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        playStopBox.items.add (juce::FlexItem (stopButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        mainBox.items.add (juce::FlexItem (recButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        mainBox.items.add (juce::FlexItem (playStopBox).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        mainBox.items.add (juce::FlexItem (playModeButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        juce::FlexBox trackSelectBox;
        trackSelectBox.flexDirection = juce::FlexBox::Direction::column;
        trackSelectBox.items.add (juce::FlexItem (prevButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        trackSelectBox.items.add (juce::FlexItem (nextButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        mainBox.items.add (juce::FlexItem (trackSelectBox).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        mainBox.performLayout (getLocalBounds().toFloat());
    }

private:
    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::RecordingStateChanged,
                                                                        EngineMessageBus::EventType::PlaybackStateChanged,
                                                                        EngineMessageBus::EventType::SinglePlayModeChanged };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::RecordingStateChanged:
            {
                bool isRecording = std::get<bool> (event.data);
                recButton.setToggleState (isRecording, juce::dontSendNotification);
                break;
            }
            case EngineMessageBus::EventType::PlaybackStateChanged:
            {
                bool isPlaying = std::get<bool> (event.data);
                playButton.setToggleState (isPlaying, juce::dontSendNotification);
                break;
            }
            case EngineMessageBus::EventType::SinglePlayModeChanged:
            {
                bool isSinglePlayMode = std::get<bool> (event.data);
                playModeButton.setToggleState (isSinglePlayMode, juce::dontSendNotification);
                break;
            }
            default:
                throw juce::String ("Unhandled event type in TransportControlsComponent" + juce::String (static_cast<int> (event.type)));
        }
    }

    juce::TextButton recButton;
    juce::TextButton playButton;
    juce::TextButton playModeButton;
    juce::TextButton stopButton;
    juce::TextButton prevButton;
    juce::TextButton nextButton;

    EngineMessageBus* uiToEngineBus;
    EngineStateToUIBridge* engineState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportControlsComponent)
};
