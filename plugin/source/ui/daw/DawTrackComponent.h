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
        trackLabel.setText ("Track " + juce::String (trackIdx + 1), juce::dontSendNotification);
        trackLabel.setFont (LooperTheme::Fonts::getBoldFont (11.0f));
        trackLabel.setJustificationType (juce::Justification::centredLeft);
        trackLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (trackLabel);

        waveformDisplay.setBridge (bridge);
        addAndMakeVisible (waveformDisplay);

        undoButton.setButtonText ("UNDO");
        undoButton.setComponentID ("undo");
        undoButton.onClick = [this]() { sendMidiMessageToEngine (UNDO_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (undoButton);

        redoButton.setButtonText ("REDO");
        redoButton.setComponentID ("redo");
        redoButton.onClick = [this]() { sendMidiMessageToEngine (REDO_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (redoButton);

        clearButton.setButtonText ("CLEAR");
        clearButton.setComponentID ("clear");
        clearButton.onClick = [this]() { sendMidiMessageToEngine (CLEAR_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (clearButton);

        volumeFader.setSliderStyle (juce::Slider::LinearHorizontal);
        volumeFader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        volumeFader.setRange (0.0, 1.0, 0.01);
        volumeFader.setValue (0.75);
        volumeFader.onValueChange = [this]() { looperEngine.setTrackVolume (trackIndex, (float) volumeFader.getValue()); };
        addAndMakeVisible (volumeFader);

        muteButton.setButtonText ("MUTE");
        muteButton.setComponentID ("mute");
        muteButton.setClickingTogglesState (true);
        muteButton.onClick = [this]() { sendMidiMessageToEngine (MUTE_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (muteButton);

        soloButton.setButtonText ("SOLO");
        soloButton.setComponentID ("solo");
        soloButton.setClickingTogglesState (true);
        soloButton.onClick = [this]() { sendMidiMessageToEngine (SOLO_BUTTON_MIDI_NOTE, NOTE_ON); };
        addAndMakeVisible (soloButton);

        // Create accent bar component
        accentBar.setInterceptsMouseClicks (true, false);
        accentBar.onClick = [this]() { looperEngine.selectTrack (trackIndex); };
        addAndMakeVisible (accentBar);

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

    int getTrackIndex() const
    {
        return trackIndex;
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
    }

    void setActive (bool shouldBeActive)
    {
        if (isActive != shouldBeActive)
        {
            isActive = shouldBeActive;
            repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        // Removed - now handled by accent bar button
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.setColour (isActive ? LooperTheme::Colors::surface.brighter (0.05f) : LooperTheme::Colors::surface);
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

        g.setColour (isActive ? LooperTheme::Colors::cyan : LooperTheme::Colors::border);
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, isActive ? 2.0f : 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4);

        // Accent bar on the very left edge
        accentBar.setBounds (bounds.removeFromLeft (28));
        bounds.removeFromLeft (2);

        juce::FlexBox mainRow;
        mainRow.flexDirection = juce::FlexBox::Direction::column;
        mainRow.alignItems = juce::FlexBox::AlignItems::stretch;

        juce::FlexBox undoRedoClearRow;
        undoRedoClearRow.flexDirection = juce::FlexBox::Direction::row;
        undoRedoClearRow.items.add (juce::FlexItem (undoButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));
        undoRedoClearRow.items.add (juce::FlexItem (redoButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        undoRedoClearRow.items.add (juce::FlexItem().withFlex (3.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        undoRedoClearRow.items.add (juce::FlexItem (clearButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));
        mainRow.items.add (juce::FlexItem (undoRedoClearRow).withFlex (0.15f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        mainRow.items.add (juce::FlexItem (waveformDisplay).withFlex (0.3f));

        juce::FlexBox muteSoloRow;
        muteSoloRow.flexDirection = juce::FlexBox::Direction::row;
        muteSoloRow.items.add (juce::FlexItem (soloButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));
        muteSoloRow.items.add (juce::FlexItem (muteButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        muteSoloRow.items.add (juce::FlexItem().withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        muteSoloRow.items.add (juce::FlexItem (volumeFader).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        mainRow.items.add (juce::FlexItem (muteSoloRow).withFlex (0.15f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        mainRow.performLayout (bounds.toFloat());
    }

private:
    class AccentBar : public juce::Component
    {
    public:
        std::function<void()> onClick;

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();
            auto* track = dynamic_cast<DawTrackComponent*> (getParentComponent());
            bool isActive = track ? track->isActive : false;

            g.setColour (isActive ? LooperTheme::Colors::cyan.withAlpha (0.8f) : LooperTheme::Colors::primary.withAlpha (0.3f));
            g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

            g.setColour (isActive ? LooperTheme::Colors::backgroundDark : LooperTheme::Colors::cyan);
            g.setFont (LooperTheme::Fonts::getBoldFont (14.0f)); // Bigger font

            // Just draw the track number, centered vertically and horizontally
            g.drawText (juce::String (track->getTrackIndex() + 1),
                        bounds,
                        juce::Justification::centred); // Changed from centredBottom to centred
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (onClick) onClick();
        }

        void mouseEnter (const juce::MouseEvent&) override
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
        }
    };

    int trackIndex;
    bool isActive = false;
    juce::Label trackLabel;
    WaveformComponent waveformDisplay;
    juce::TextButton undoButton;
    juce::TextButton redoButton;
    juce::TextButton clearButton;
    juce::Slider volumeFader;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    AccentBar accentBar;

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
