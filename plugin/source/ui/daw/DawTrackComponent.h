#pragma once
#include "engine/LooperEngine.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/WaveformComponent.h"
#include "ui/daw/PlaybackSlider.h"
#include "ui/daw/VolumesComponent.h"
#include <JuceHeader.h>

class DawTrackComponent : public juce::Component, private juce::Timer
{
public:
    DawTrackComponent (LooperEngine* engine, int trackIdx, AudioToUIBridge* bridge)
        : trackIndex (trackIdx), looperEngine (engine), volumesComponent (engine, trackIdx, bridge)
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
        undoButton.onClick = [this]() { sendCommandToEngine (MidiNotes::UNDO_BUTTON); };
        addAndMakeVisible (undoButton);

        redoButton.setButtonText ("REDO");
        redoButton.setComponentID ("redo");
        redoButton.onClick = [this]() { sendCommandToEngine (MidiNotes::REDO_BUTTON); };
        addAndMakeVisible (redoButton);

        clearButton.setButtonText ("CLEAR");
        clearButton.setComponentID ("clear");
        clearButton.onClick = [this]()
        {
            sendCommandToEngine (MidiNotes::CLEAR_BUTTON);
            waveformDisplay.clearTrack();
        };
        addAndMakeVisible (clearButton);

        volumeFader.setSliderStyle (juce::Slider::LinearHorizontal);
        volumeFader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        volumeFader.setRange (0.0, 1.0, 0.01);
        volumeFader.setValue (0.75);
        volumeFader.onValueChange = [this]() { sendCommandToEngine (MidiNotes::TRACK_VOLUME_CC, volumeFader.getValue()); };
        addAndMakeVisible (volumeFader);

        muteButton.setButtonText ("MUTE");
        muteButton.setComponentID ("mute");
        muteButton.setClickingTogglesState (true);
        muteButton.onClick = [this]() { sendCommandToEngine (MidiNotes::MUTE_BUTTON); };
        addAndMakeVisible (muteButton);

        soloButton.setButtonText ("SOLO");
        soloButton.setComponentID ("solo");
        soloButton.setClickingTogglesState (true);
        soloButton.onClick = [this]() { sendCommandToEngine (MidiNotes::SOLO_BUTTON); };
        addAndMakeVisible (soloButton);

        // Create accent bar component
        accentBar.setInterceptsMouseClicks (true, false);
        accentBar.onClick = [this]() { sendCommandToEngine (MidiNotes::TRACK_SELECT_CC, trackIndex); };
        addAndMakeVisible (accentBar);

        reverseButton.setButtonText ("REV");
        reverseButton.setComponentID ("reverse");
        reverseButton.setClickingTogglesState (true);
        reverseButton.onClick = [this]() { sendCommandToEngine (MidiNotes::REVERSE_BUTTON); };
        addAndMakeVisible (reverseButton);

        keepPitchButton.setButtonText ("PITCH");
        keepPitchButton.setComponentID ("keepPitch");
        keepPitchButton.setClickingTogglesState (true);
        keepPitchButton.onClick = [this]() { sendCommandToEngine (MidiNotes::KEEP_PITCH_BUTTON); };
        addAndMakeVisible (keepPitchButton);

        speedFader.setSliderStyle (juce::Slider::LinearHorizontal);
        speedFader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        speedFader.setValue (1.0);
        speedFader.onValueChange = [this]() { sendCommandToEngine (MidiNotes::PLAYBACK_SPEED_CC, speedFader.getValue()); };
        addAndMakeVisible (speedFader);

        volumesComponent.setComponentID ("volumesComponent");
        addAndMakeVisible (volumesComponent);

        updateControlsFromEngine();
        startTimerHz (10);
    }

    ~DawTrackComponent() override { stopTimer(); }

    void timerCallback() override { updateControlsFromEngine(); }

    int getTrackIndex() const { return trackIndex; }

    void updateControlsFromEngine()
    {
        auto* track = looperEngine->getTrackByIndex (trackIndex);
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

        accentBar.repaint();
    }

    void setActive (bool shouldBeActive)
    {
        if (isActive != shouldBeActive)
        {
            isActive = shouldBeActive;
            repaint();
        }
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

        juce::FlexBox reverseSpeedRow;
        reverseSpeedRow.flexDirection = juce::FlexBox::Direction::row;
        reverseSpeedRow.items.add (juce::FlexItem (reverseButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));
        reverseSpeedRow.items.add (juce::FlexItem (keepPitchButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        reverseSpeedRow.items.add (juce::FlexItem().withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        reverseSpeedRow.items.add (juce::FlexItem (speedFader).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        mainRow.items.add (juce::FlexItem (reverseSpeedRow).withFlex (0.15f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        juce::FlexBox muteSoloRow;
        muteSoloRow.flexDirection = juce::FlexBox::Direction::row;
        muteSoloRow.items.add (juce::FlexItem (soloButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));
        muteSoloRow.items.add (juce::FlexItem (muteButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        muteSoloRow.items.add (juce::FlexItem (volumesComponent).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        muteSoloRow.items.add (juce::FlexItem (volumeFader).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        mainRow.items.add (juce::FlexItem (muteSoloRow).withFlex (0.15f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        mainRow.performLayout (bounds.toFloat());
    }

private:
    class AccentBar : public juce::Component
    {
    public:
        std::function<void()> onClick;
        AccentBar() {}

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();
            auto* track = dynamic_cast<DawTrackComponent*> (getParentComponent());
            bool isTrackActive = track ? track->isActive : false;

            // Check for pending track change
            int pendingIndex = track->looperEngine->getPendingTrackIndex();
            bool isPendingTrack = (pendingIndex == track->getTrackIndex());

            // Choose color
            if (isPendingTrack && ! isTrackActive)
                g.setColour (LooperTheme::Colors::yellow.withAlpha (0.8f));
            else if (isTrackActive)
                g.setColour (LooperTheme::Colors::cyan.withAlpha (0.8f));
            else
                g.setColour (LooperTheme::Colors::primary.withAlpha (0.3f));

            // ACTUALLY DRAW IT!
            g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

            // Draw track number
            g.setColour (isTrackActive ? LooperTheme::Colors::backgroundDark : LooperTheme::Colors::cyan);
            g.setFont (LooperTheme::Fonts::getBoldFont (14.0f));
            g.drawText (juce::String (track->getTrackIndex() + 1), bounds, juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (onClick) onClick();
        }

        void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void mouseExit (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::NormalCursor); }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AccentBar)
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
    juce::TextButton reverseButton;
    juce::TextButton keepPitchButton;
    PlaybackSpeedSlider speedFader;

    VolumesComponent volumesComponent;

    LooperEngine* looperEngine;

    /// Sends a command to the engine using MIDI protocol.
    /// This ensures UI commands go through the same validation,
    /// logging, and dispatch path as external MIDI controllers.
    void sendCommandToEngine (const int noteNumber, const bool isNoteOn = true)
    {
        juce::MidiBuffer midiBuffer;
        juce::MidiMessage msg = isNoteOn ? juce::MidiMessage::noteOn (1, noteNumber, (juce::uint8) 100)
                                         : juce::MidiMessage::noteOff (1, noteNumber);

        midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
        midiBuffer.addEvent (msg, 0);
        looperEngine->handleMidiCommand (midiBuffer);
    }

    /// Sends a command to the engine using MIDI protocol.
    /// This ensures UI commands go through the same validation,
    /// logging, and dispatch path as external MIDI controllers.
    void sendCommandToEngine (const int controllerNumber, const int value)
    {
        juce::MidiBuffer midiBuffer;
        midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
        midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, controllerNumber, value), 0);
        looperEngine->handleMidiCommand (midiBuffer);
    }

    /// Sends a command to the engine using MIDI protocol.
    /// This ensures UI commands go through the same validation,
    /// logging, and dispatch path as external MIDI controllers.
    void sendCommandToEngine (const int controllerNumber, const double value)
    {
        juce::MidiBuffer midiBuffer;

        int ccValue = (int) std::clamp (value * 127.0, 0.0, 127.0);

        if (controllerNumber == MidiNotes::PLAYBACK_SPEED_CC) ccValue = (int) (((value - 0.2) / 1.8) * 127.0);

        midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
        midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, controllerNumber, ccValue), 0);
        looperEngine->handleMidiCommand (midiBuffer);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DawTrackComponent)
};
