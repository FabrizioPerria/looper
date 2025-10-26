#pragma once
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/TrackComponent.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class LooperEditor : public juce::Component, public juce::Timer
{
public:
    LooperEditor (LooperEngine* engine) : looperEngine (engine), midiDispatcher (engine)
    {
        for (int i = 0; i < engine->getNumTracks(); ++i)
        {
            auto* channel = new TrackComponent (engine, i, engine->getUIBridgeByIndex (i));
            channels.add (channel);
            addAndMakeVisible (channel);
        }

        recordButton.setButtonText ("REC");
        recordButton.setClickingTogglesState (true);
        recordButton.onClick = [this]() { midiDispatcher.sendCommandToEngine (MidiNotes::TOGGLE_RECORD_BUTTON); };
        addAndMakeVisible (recordButton);

        playButton.setButtonText ("PLAY");
        playButton.setClickingTogglesState (true);
        playButton.onClick = [this]() { midiDispatcher.sendCommandToEngine (MidiNotes::TOGGLE_PLAY_BUTTON); };
        addAndMakeVisible (playButton);

        prevButton.setButtonText ("PREV");
        prevButton.onClick = [this]() { midiDispatcher.sendCommandToEngine (MidiNotes::PREV_TRACK); };
        addAndMakeVisible (prevButton);

        nextButton.setButtonText ("NEXT");
        nextButton.onClick = [this]() { midiDispatcher.sendCommandToEngine (MidiNotes::NEXT_TRACK); };
        addAndMakeVisible (nextButton);

        startTimerHz (10);
    }

    ~LooperEditor() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (LooperTheme::Colors::backgroundDark);

        // Top bar
        auto topBar = getLocalBounds().removeFromTop (50);
        g.setColour (LooperTheme::Colors::surface);
        g.fillRect (topBar);

        // Bottom border for top bar
        g.setColour (LooperTheme::Colors::border);
        g.drawLine (0, 50, (float) getWidth(), 50, 1.0f);

        // Draw title text
        g.setColour (LooperTheme::Colors::cyan);
        g.setFont (LooperTheme::Fonts::getTitleFont (18.0f));
        g.drawText ("LOOPER", juce::Rectangle<float> (12, 8, 150, 34), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Top bar with transport controls
        auto topBar = bounds.removeFromTop (50);
        topBar.reduce (12, 8);

        // Transport buttons in a horizontal row
        auto transportBounds = topBar.withSizeKeepingCentre (300, 34);

        juce::FlexBox transportFlex;
        transportFlex.flexDirection = juce::FlexBox::Direction::row;
        transportFlex.justifyContent = juce::FlexBox::JustifyContent::center;
        transportFlex.alignItems = juce::FlexBox::AlignItems::center;

        transportFlex.items
            .add (juce::FlexItem (recordButton).withWidth (70.0f).withHeight (34.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 0)));
        transportFlex.items
            .add (juce::FlexItem (playButton).withWidth (70.0f).withHeight (34.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        transportFlex.items
            .add (juce::FlexItem (prevButton).withWidth (70.0f).withHeight (34.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        transportFlex.items
            .add (juce::FlexItem (nextButton).withWidth (70.0f).withHeight (34.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 4)));

        transportFlex.performLayout (transportBounds.toFloat());

        bounds.removeFromTop (8);
        bounds.reduce (8, 0);

        juce::FlexBox tracksFlex;
        tracksFlex.flexDirection = juce::FlexBox::Direction::column;
        tracksFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        for (auto* channel : channels)
        {
            tracksFlex.items.add (juce::FlexItem (*channel).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 4, 0)));
        }

        tracksFlex.performLayout (bounds.toFloat());
    }

private:
    LooperEngine* looperEngine;
    MidiCommandDispatcher midiDispatcher;

    juce::OwnedArray<TrackComponent> channels;

    juce::TextButton recordButton;
    juce::TextButton playButton;
    juce::TextButton nextButton;
    juce::TextButton prevButton;

    void timerCallback() override
    {
        auto state = looperEngine->getState();

        // Update button states based on new state enum
        recordButton.setToggleState (state == LooperState::Recording || state == LooperState::Overdubbing, juce::dontSendNotification);

        playButton.setToggleState (state == LooperState::Playing || state == LooperState::PendingTrackChange
                                       || state == LooperState::Overdubbing,
                                   juce::dontSendNotification);

        // Update active track highlighting
        int activeTrackIndex = looperEngine->getActiveTrackIndex();
        for (int i = 0; i < channels.size(); ++i)
        {
            channels[i]->setActive (i == activeTrackIndex);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperEditor)
};
