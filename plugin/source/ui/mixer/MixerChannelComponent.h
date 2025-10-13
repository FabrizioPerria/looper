#pragma once
#include "LooperTheme.h"
#include "engine/LooperEngine.h"
#include "engine/midiMappings.h"
#include "ui/components/WaveformComponent.h"
#include <JuceHeader.h>

class MixerChannelComponent : public juce::Component, private juce::Timer
{
public:
    MixerChannelComponent (LooperEngine& engine, int trackIdx, AudioToUIBridge* bridge) : trackIndex (trackIdx), looperEngine (engine)
    {
        // Track label
        trackLabel.setText ("T" + juce::String (trackIdx), juce::dontSendNotification);
        trackLabel.setFont (LooperTheme::Fonts::getBoldFont (11.0f));
        trackLabel.setJustificationType (juce::Justification::centred);
        trackLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (trackLabel);

        waveformDisplay.setBridge (bridge);
        addAndMakeVisible (waveformDisplay);

        undoButton.setButtonText ("U");
        undoButton.onClick = [this]() { sendMidiMessageToEngine (UNDO_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (undoButton);

        redoButton.setButtonText ("R");
        redoButton.onClick = [this]() { sendMidiMessageToEngine (REDO_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (redoButton);

        clearButton.setButtonText ("C");
        clearButton.onClick = [this]() { sendMidiMessageToEngine (CLEAR_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (clearButton);

        volumeFader.setSliderStyle (juce::Slider::LinearVertical);
        volumeFader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        volumeFader.setRange (0.0, 1.0, 0.01);
        volumeFader.setValue (0.75);
        volumeFader.onValueChange = [this]() { looperEngine.setTrackVolume (trackIndex, (float) volumeFader.getValue()); };
        addAndMakeVisible (volumeFader);

        // Mute button
        muteButton.setButtonText ("M");
        muteButton.setClickingTogglesState (true);
        muteButton.onClick = [this]() { sendMidiMessageToEngine (MUTE_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (muteButton);

        // Solo button
        soloButton.setButtonText ("S");
        soloButton.setClickingTogglesState (true);
        soloButton.onClick = [this]() { sendMidiMessageToEngine (SOLO_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (soloButton);

        updateControlsFromEngine();
        startTimerHz (10);
    }

    ~MixerChannelComponent() override
    {
        stopTimer();
    }

    void timerCallback()
    {
        updateControlsFromEngine();
    }

    void updateControlsFromEngine()
    {
        auto* track = looperEngine.getTrackByIndex (trackIndex);
        if (! track) return;

        // Update volume slider if it changed (avoid feedback loop)
        float currentVolume = track->getTrackVolume();
        if (std::abs (volumeFader.getValue() - currentVolume) > 0.001)
        {
            volumeFader.setValue (currentVolume, juce::dontSendNotification);
        }

        // Update mute button
        bool currentMuted = track->isMuted();
        if (muteButton.getToggleState() != currentMuted)
        {
            muteButton.setToggleState (currentMuted, juce::dontSendNotification);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Channel strip background
        g.setColour (LooperTheme::Colors::surface);
        g.fillRoundedRectangle (bounds.toFloat(), 6.0f);

        // Border
        g.setColour (LooperTheme::Colors::border);
        g.drawRoundedRectangle (bounds.toFloat().reduced (1), 6.0f, 1.5f);

        // Top accent line
        g.setColour (LooperTheme::Colors::primary.withAlpha (0.3f));
        g.fillRoundedRectangle (bounds.removeFromTop (3).toFloat(), 6.0f);
    }

    void resized() override
    {
        juce::FlexBox mainFlex;
        mainFlex.flexDirection = juce::FlexBox::Direction::column;
        mainFlex.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        mainFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        auto bounds = getLocalBounds().reduced (6).toFloat();

        mainFlex.items.add (juce::FlexItem (trackLabel).withHeight (20.0f).withMargin (juce::FlexItem::Margin (0, 0, 4, 0)));

        juce::FlexBox buttonFlex;
        buttonFlex.flexDirection = juce::FlexBox::Direction::row;
        buttonFlex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
        buttonFlex.items.add (juce::FlexItem (undoButton).withFlex (1).withMargin (juce::FlexItem::Margin (0, 2, 0, 0)));
        buttonFlex.items.add (juce::FlexItem (redoButton).withFlex (1).withMargin (juce::FlexItem::Margin (0, 2, 0, 2)));
        buttonFlex.items.add (juce::FlexItem (clearButton).withFlex (1).withMargin (juce::FlexItem::Margin (0, 0, 0, 2)));

        mainFlex.items.add (juce::FlexItem (buttonFlex).withHeight (20.0f).withMargin (juce::FlexItem::Margin (0, 0, 4, 0)));

        mainFlex.items.add (juce::FlexItem (volumeFader).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 8, 0)));

        mainFlex.items.add (juce::FlexItem (muteButton).withHeight (22.0f).withMargin (juce::FlexItem::Margin (0, 0, 4, 0)));

        mainFlex.items.add (juce::FlexItem (soloButton).withHeight (22.0f).withMargin (juce::FlexItem::Margin (0, 0, 8, 0)));

        mainFlex.items.add (juce::FlexItem (waveformDisplay).withHeight (60.0f));

        mainFlex.performLayout (bounds);
    }

private:
    int trackIndex;
    juce::Label trackLabel;
    WaveformComponent waveformDisplay;
    juce::TextButton undoButton;
    juce::TextButton redoButton;
    juce::TextButton clearButton;
    juce::Slider volumeFader;
    juce::TextButton muteButton;
    juce::TextButton soloButton;

    LooperEngine& looperEngine;

    void sendMidiMessageToEngine (const int noteNumber, const bool isNoteOn)
    {
        juce::MidiBuffer midiBuffer;
        juce::MidiMessage msg = isNoteOn ? juce::MidiMessage::noteOn (1, noteNumber, (juce::uint8) 100)
                                         : juce::MidiMessage::noteOff (1, noteNumber);
        midiBuffer.addEvent (msg, 0);
        looperEngine.handleMidiCommand (midiBuffer);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerChannelComponent)
};
