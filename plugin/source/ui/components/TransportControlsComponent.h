#pragma once

#include "audio/EngineStateToUIBridge.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class TransportControlsComponent : public juce::Component, private juce::Timer
{
public:
    TransportControlsComponent (MidiCommandDispatcher* midiCommandDispatcher, EngineStateToUIBridge* bridge)
        : midiDispatcher (midiCommandDispatcher), engineState (bridge)
    {
        recButton.setButtonText ("REC");
        recButton.setComponentID ("rec");
        recButton.setClickingTogglesState (true);
        recButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::TOGGLE_RECORD_BUTTON); };
        addAndMakeVisible (recButton);

        playButton.setButtonText ("PLAY");
        playButton.setComponentID ("play");
        playButton.setClickingTogglesState (true);
        playButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::TOGGLE_PLAY_BUTTON); };
        addAndMakeVisible (playButton);

        prevButton.setButtonText ("PREV");
        prevButton.setComponentID ("prev");
        prevButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::PREV_TRACK); };
        addAndMakeVisible (prevButton);

        nextButton.setButtonText ("NEXT");
        nextButton.setComponentID ("next");
        nextButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::NEXT_TRACK); };
        addAndMakeVisible (nextButton);
        startTimerHz (10);
    }

    ~TransportControlsComponent() override { stopTimer(); }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int buttonWidth = bounds.getWidth() / 4;
        recButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        playButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        prevButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        nextButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));
        g.drawLine (bounds.getX(), bounds.getY() + 8, bounds.getX(), bounds.getBottom() - 8, 1.0f);
        g.drawLine (bounds.getRight() - 1, bounds.getY() + 8, bounds.getRight() - 1, bounds.getBottom() - 8, 1.0f);
    }

private:
    void timerCallback() override
    {
        bool recording, playing;
        int activeTrack, pendingTrack, numTracks;
        engineState->getEngineState (recording, playing, activeTrack, pendingTrack, numTracks);

        // Update button states based on new state enum
        recButton.setToggleState (recording, juce::dontSendNotification);
        playButton.setToggleState (playing, juce::dontSendNotification);
    }

    juce::TextButton recButton;
    juce::TextButton playButton;
    juce::TextButton prevButton;
    juce::TextButton nextButton;

    MidiCommandDispatcher* midiDispatcher;
    EngineStateToUIBridge* engineState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportControlsComponent)
};
