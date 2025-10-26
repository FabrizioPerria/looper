#pragma once

#include "engine/MidiCommandConfig.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class TransportControlsComponent : public juce::Component, private juce::Timer
{
public:
    TransportControlsComponent (MidiCommandDispatcher* midiCommandDispatcher) : midiDispatcher (midiCommandDispatcher)
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
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int buttonWidth = bounds.getWidth() / 4;
        recButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        playButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        prevButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
        nextButton.setBounds (bounds.removeFromLeft (buttonWidth).reduced (2));
    }

private:
    void timerCallback() override
    {
        auto state = midiDispatcher->getCurrentState();

        // Update button states based on new state enum
        recButton.setToggleState (state == LooperState::Recording || state == LooperState::Overdubbing, juce::dontSendNotification);

        playButton.setToggleState (state == LooperState::Playing || state == LooperState::PendingTrackChange
                                       || state == LooperState::Overdubbing,
                                   juce::dontSendNotification);
    }

    juce::TextButton recButton;
    juce::TextButton playButton;
    juce::TextButton prevButton;
    juce::TextButton nextButton;

    MidiCommandDispatcher* midiDispatcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportControlsComponent)
};
