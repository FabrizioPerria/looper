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
        // recButton.setClickingTogglesState (true);
        recButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleRecord, -1, {} }); };
        addAndMakeVisible (recButton);

        playButton.setButtonText ("PLAY");
        playButton.setComponentID ("play");
        // playButton.setClickingTogglesState (true);
        playButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::TogglePlay, -1, {} }); };
        addAndMakeVisible (playButton);

        stopButton.setButtonText ("STOP");
        stopButton.setComponentID ("stop");
        // playButton.setClickingTogglesState (true);
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
        auto bounds = getLocalBounds();
        int buttonWidth = bounds.getWidth() / 5;
        recButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        playButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        stopButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        prevButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        nextButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
    }

    void paint (juce::Graphics& g) override
    {
        // auto bounds = getLocalBounds().toFloat();
        // g.setColour (LooperTheme::Colors::surface.brighter (0.2f));
        // g.drawLine (bounds.getX(), bounds.getY() + 8, bounds.getX(), bounds.getBottom() - 8, 1.0f);
        // g.drawLine (bounds.getRight(), bounds.getY() + 8, bounds.getRight(), bounds.getBottom() - 8, 1.0f);
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

    EngineMessageBus* uiToEngineBus;
    EngineStateToUIBridge* engineState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportControlsComponent)
};
