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
        transportLabel.setText ("Transport", juce::dontSendNotification);
        transportLabel.setJustificationType (juce::Justification::centred);
        transportLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (transportLabel);

        trackLabel.setText ("Track", juce::dontSendNotification);
        trackLabel.setJustificationType (juce::Justification::centred);
        trackLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (trackLabel);

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

        stopButton.setButtonText ("STOP");
        stopButton.setComponentID ("stop");
        stopButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::Stop, -1, {} }); };
        addAndMakeVisible (stopButton);

        prevButton.setButtonText ("PREV");
        prevButton.setComponentID ("prevTrack");
        prevButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::PreviousTrack, -1, {} }); };
        addAndMakeVisible (prevButton);

        nextButton.setButtonText ("NEXT");
        nextButton.setComponentID ("nextTrack");
        nextButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::NextTrack, -1, {} }); };
        addAndMakeVisible (nextButton);

        uiToEngineBus->addListener (this);
    }

    ~TransportControlsComponent() override { uiToEngineBus->removeListener (this); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));

        auto titleBounds = transportLabel.getBounds();
        g.fillRect (titleBounds.getX() + 3.0f, titleBounds.getBottom() + 3.0f, titleBounds.getWidth() - 6.0f, 1.0f);

        titleBounds = trackLabel.getBounds();
        g.fillRect (titleBounds.getX() + 3.0f, titleBounds.getBottom() + 3.0f, titleBounds.getWidth() - 6.0f, 1.0f);
    }

    void resized() override
    {
        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::row;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;

        juce::FlexBox playStopRecBox;
        playStopRecBox.flexDirection = juce::FlexBox::Direction::row;
        playStopRecBox.items.add (juce::FlexItem (recButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 1, 0, 1)));
        playStopRecBox.items.add (juce::FlexItem (playButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 1, 0, 1)));
        playStopRecBox.items.add (juce::FlexItem (stopButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 1, 0, 1)));

        juce::FlexBox transportBox;
        transportBox.flexDirection = juce::FlexBox::Direction::column;
        transportBox.alignItems = juce::FlexBox::AlignItems::stretch;
        transportBox.items.add (juce::FlexItem (transportLabel).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        transportBox.items.add (juce::FlexItem (playStopRecBox).withFlex (3.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        juce::FlexBox trackSelectBox;
        trackSelectBox.flexDirection = juce::FlexBox::Direction::row;
        trackSelectBox.items.add (juce::FlexItem (prevButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 1, 0, 1)));
        trackSelectBox.items.add (juce::FlexItem (nextButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 1, 0, 1)));

        juce::FlexBox trackBox;
        trackBox.flexDirection = juce::FlexBox::Direction::column;
        trackBox.alignItems = juce::FlexBox::AlignItems::stretch;
        trackBox.items.add (juce::FlexItem (trackLabel).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        trackBox.items.add (juce::FlexItem (trackSelectBox).withFlex (3.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        mainBox.items.add (juce::FlexItem (transportBox).withFlex (0.6f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        mainBox.items.add (juce::FlexItem (trackBox).withFlex (0.3f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        mainBox.performLayout (getLocalBounds().toFloat());
    }

private:
    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::RecordingStateChanged,
                                                                        EngineMessageBus::EventType::PlaybackStateChanged };

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
            default:
                throw juce::String ("Unhandled event type in TransportControlsComponent" + juce::String (static_cast<int> (event.type)));
        }
    }

    juce::TextButton recButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton prevButton;
    juce::TextButton nextButton;

    juce::Label transportLabel { "Transport", "Transport" };
    juce::Label trackLabel { "Track", "Track" };

    EngineMessageBus* uiToEngineBus;
    EngineStateToUIBridge* engineState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportControlsComponent)
};
