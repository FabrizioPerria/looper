#pragma once
#include "engine/LooperEngine.h"
#include "engine/midiMappings.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/WaveformComponent.h"
#include <JuceHeader.h>

class DawTrackComponent : public juce::Component, private juce::Timer
{
public:
    DawTrackComponent (LooperEngine& engine, int trackIdx, AudioToUIBridge* bridge) : trackIndex (trackIdx), looperEngine (engine)
    {
        // Track label
        trackLabel.setText ("T" + juce::String (trackIdx), juce::dontSendNotification);
        trackLabel.setFont (LooperTheme::Fonts::getBoldFont (11.0f));
        trackLabel.setJustificationType (juce::Justification::centredLeft);
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

        volumeFader.setSliderStyle (juce::Slider::LinearHorizontal);
        volumeFader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        volumeFader.setRange (0.0, 1.0, 0.01);
        volumeFader.setValue (0.75);
        volumeFader.onValueChange = [this]() { looperEngine.setTrackVolume (trackIndex, (float) volumeFader.getValue()); };
        addAndMakeVisible (volumeFader);

        muteButton.setButtonText ("M");
        muteButton.setClickingTogglesState (true);
        muteButton.onClick = [this]() { sendMidiMessageToEngine (MUTE_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (muteButton);

        soloButton.setButtonText ("S");
        soloButton.setClickingTogglesState (true);
        soloButton.onClick = [this]() { sendMidiMessageToEngine (SOLO_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (soloButton);

        updateControlsFromEngine();
        startTimerHz (10);
    }

    ~DawTrackComponent() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        updateControlsFromEngine();
    }

    void updateControlsFromEngine()
    {
        auto* track = looperEngine.getTrackByIndex (trackIndex);
        if (! track) return;

        float currentVolume = track->getTrackVolume();
        if (std::abs (volumeFader.getValue() - currentVolume) > 0.001)
        {
            volumeFader.setValue (currentVolume, juce::dontSendNotification);
        }

        bool currentMuted = track->isMuted();
        if (muteButton.getToggleState() != currentMuted)
        {
            muteButton.setToggleState (currentMuted, juce::dontSendNotification);
        }

        static int lastActiveTrack = -1;
        int currentActiveTrack = looperEngine.getActiveTrackIndex();
        if (lastActiveTrack != currentActiveTrack)
        {
            repaint();
            lastActiveTrack = currentActiveTrack;
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        looperEngine.selectTrack (trackIndex);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        bool isActive = (looperEngine.getActiveTrackIndex() == trackIndex);

        // Track background
        g.setColour (isActive ? LooperTheme::Colors::surface.brighter (0.15f) : LooperTheme::Colors::surface);
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

        // Border
        g.setColour (isActive ? LooperTheme::Colors::cyan : LooperTheme::Colors::border);
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, isActive ? 2.0f : 1.0f);

        // Left accent bar
        g.setColour (isActive ? LooperTheme::Colors::cyan.withAlpha (0.6f) : LooperTheme::Colors::primary.withAlpha (0.3f));
        g.fillRoundedRectangle (bounds.removeFromLeft (3).toFloat(), 4.0f);
    }

    void resized() override
    {
        juce::FlexBox flexMain;
        flexMain.flexDirection = juce::FlexBox::Direction::row;
        flexMain.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        flexMain.alignItems = juce::FlexBox::AlignItems::stretch;

        juce::FlexBox leftColumn;
        leftColumn.flexDirection = juce::FlexBox::Direction::column;
        leftColumn.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        leftColumn.alignItems = juce::FlexBox::AlignItems::stretch;
        leftColumn.items.add (juce::FlexItem (trackLabel).withHeight (20.0f).withMargin (juce::FlexItem::Margin (0, 0, 4, 0)));
        leftColumn.items.add (juce::FlexItem (undoButton).withHeight (18.0f).withMargin (juce::FlexItem::Margin (0, 0, 2, 0)));
        leftColumn.items.add (juce::FlexItem (redoButton).withHeight (18.0f).withMargin (juce::FlexItem::Margin (0, 0, 2, 0)));
        leftColumn.items.add (juce::FlexItem (clearButton).withHeight (18.0f).withMargin (juce::FlexItem::Margin (0, 0, 4, 0)));
        leftColumn.items.add (juce::FlexItem (muteButton).withHeight (18.0f).withMargin (juce::FlexItem::Margin (0, 0, 2, 0)));
        leftColumn.items.add (juce::FlexItem (soloButton).withHeight (18.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 0)));

        flexMain.items.add (juce::FlexItem (leftColumn).withWidth (60.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 0)));
        flexMain.items.add (juce::FlexItem (waveformDisplay).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 4, 24, 0)));
        flexMain.items.add (juce::FlexItem (volumeFader).withHeight (20.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 0)));

        flexMain.performLayout (getLocalBounds().toFloat().reduced (4.0f));
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

        midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, TRACK_SELECT_CC, trackIndex), 0);
        midiBuffer.addEvent (msg, 0);
        looperEngine.handleMidiCommand (midiBuffer);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DawTrackComponent)
};
