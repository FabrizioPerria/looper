#pragma once
#include "engine/LooperEngine.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class VolumesComponent : public juce::Component, private juce::Timer
{
public:
    VolumesComponent (LooperEngine* engine, int trackIdx) : trackIndex (trackIdx), looperEngine (engine), midiDispatcher (engine)
    {
        normalizeButton.setButtonText ("NORM");
        normalizeButton.setClickingTogglesState (true);
        normalizeButton.onClick = [this]() { midiDispatcher.sendCommandToEngine (MidiNotes::VOLUME_NORMALIZE_BUTTON, trackIndex); };
        addAndMakeVisible (normalizeButton);

        overdubLevelKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        overdubLevelKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        overdubLevelKnob.setRange (0.0, 1.0, 0.01);
        overdubLevelKnob.setValue (0.5);
        overdubLevelKnob.setPopupDisplayEnabled (true, true, nullptr);
        overdubLevelKnob.onValueChange = [this]()
        { midiDispatcher.sendControlChangeToEngine (MidiNotes::OVERDUB_LEVEL_CC, trackIndex, overdubLevelKnob.getValue()); };
        addAndMakeVisible (overdubLevelKnob);

        existingAudioLevelKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        existingAudioLevelKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        existingAudioLevelKnob.setRange (0.0, 1.0, 0.01);
        existingAudioLevelKnob.setValue (0.5);
        existingAudioLevelKnob.setPopupDisplayEnabled (true, true, nullptr);
        existingAudioLevelKnob.onValueChange = [this]()
        { midiDispatcher.sendControlChangeToEngine (MidiNotes::EXISTING_AUDIO_LEVEL_CC, trackIndex, existingAudioLevelKnob.getValue()); };
        addAndMakeVisible (existingAudioLevelKnob);

        overdubLabel.setText ("NEXT", juce::dontSendNotification);
        overdubLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        overdubLabel.setJustificationType (juce::Justification::centred);
        overdubLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (overdubLabel);

        existingLabel.setText ("LOOP", juce::dontSendNotification);
        existingLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        existingLabel.setJustificationType (juce::Justification::centred);
        existingLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (existingLabel);

        startTimerHz (10);
    }

    ~VolumesComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (LooperTheme::Colors::backgroundDark.withAlpha (0.3f));
        g.fillRoundedRectangle (bounds, 3.0f);
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
    int trackIndex;
    bool isUpdatingFromEngine = false;

    juce::TextButton normalizeButton;
    juce::Slider overdubLevelKnob;
    juce::Slider existingAudioLevelKnob;
    juce::Label overdubLabel;
    juce::Label existingLabel;

    LooperEngine* looperEngine;
    MidiCommandDispatcher midiDispatcher;

    void timerCallback() override { updateFromEngine(); }

    void updateFromEngine()
    {
        auto* track = looperEngine->getTrackByIndex (trackIndex);
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
