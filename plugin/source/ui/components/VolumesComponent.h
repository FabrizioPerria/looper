#pragma once
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/LevelComponent.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class VolumesComponent : public juce::Component, private juce::Timer
{
public:
    VolumesComponent (MidiCommandDispatcher* dispatcher, int trackIdx)
        : midiDispatcher (dispatcher)
        , trackIndex (trackIdx)
        , overdubLevelKnob (midiDispatcher, trackIndex, "OVERDUB LEVEL", MidiNotes::OVERDUB_LEVEL_CC)
        , existingAudioLevelKnob (midiDispatcher, trackIndex, "EXISTING LEVEL", MidiNotes::EXISTING_AUDIO_LEVEL_CC)
    {
        normalizeButton.setButtonText ("NORM");
        normalizeButton.setComponentID ("normalize");
        normalizeButton.setClickingTogglesState (true);
        normalizeButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::VOLUME_NORMALIZE_BUTTON, trackIndex); };
        addAndMakeVisible (normalizeButton);

        addAndMakeVisible (overdubLevelKnob);
        addAndMakeVisible (existingAudioLevelKnob);

        startTimerHz (10);
    }

    ~VolumesComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));
        g.drawLine (bounds.getX(), bounds.getY() + 8, bounds.getX(), bounds.getBottom() - 8, 1.0f);
        g.drawLine (bounds.getRight() - 1, bounds.getY() + 8, bounds.getRight() - 1, bounds.getBottom() - 8, 1.0f);
    }

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
        existingFlex.items.add (juce::FlexItem (existingLabel).withFlex (0.2f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        mainRow.items.add (juce::FlexItem (existingFlex).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));

        mainRow.items.add (juce::FlexItem (normalizeButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));

        juce::FlexBox overdubFlex;
        overdubFlex.flexDirection = juce::FlexBox::Direction::column;
        overdubFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        overdubFlex.items.add (juce::FlexItem (overdubLevelKnob).withFlex (1.0f));
        overdubFlex.items.add (juce::FlexItem (overdubLabel).withFlex (0.2f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        mainRow.items.add (juce::FlexItem (overdubFlex).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        mainRow.performLayout (bounds.toFloat());
    }

private:
    MidiCommandDispatcher* midiDispatcher;
    int trackIndex;
    bool isUpdatingFromEngine = false;

    juce::TextButton normalizeButton;
    LevelComponent overdubLevelKnob;
    LevelComponent existingAudioLevelKnob;
    juce::Label overdubLabel;
    juce::Label existingLabel;

    void timerCallback() override { updateFromEngine(); }

    void updateFromEngine()
    {
        auto* track = midiDispatcher->getTrackByIndex (trackIndex);
        if (! track) return;

        isUpdatingFromEngine = true;

        normalizeButton.setToggleState (track->isOutputNormalized(), juce::dontSendNotification);

        double newGain = track->getOverdubGainNew();
        double oldGain = track->getOverdubGainOld();

        if (std::abs (overdubLevelKnob.getValue() - newGain / 2.0) > 0.01)
            overdubLevelKnob.setValue (newGain / 2.0, juce::dontSendNotification);

        if (std::abs (existingAudioLevelKnob.getValue() - oldGain / 2.0) > 0.01)
            existingAudioLevelKnob.setValue (oldGain / 2.0, juce::dontSendNotification);

        isUpdatingFromEngine = false;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumesComponent)
};
